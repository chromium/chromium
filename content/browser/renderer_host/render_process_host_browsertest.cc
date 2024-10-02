// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_process_host.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/test_timeouts.h"
#include "base/threading/hang_watcher.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_process_host_internal_observer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pseudonymization_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_service.mojom.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/storage_partition_test_helpers.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/mojo/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_WIN)
#include "base/features.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/platform/platform_handle_security_util_win.h"
#include "sandbox/policy/switches.h"
#endif

using testing::_;
using testing::InSequence;
using testing::Mock;
using testing::StrictMock;
using testing::Test;

namespace content {

namespace {

// Similar to net::test_server::DelayedHttpResponse, but the delay is resolved
// through Resolver.
class DelayedHttpResponseWithResolver final
    : public net::test_server::BasicHttpResponse {
 public:
  class Resolver final : public base::RefCountedThreadSafe<Resolver> {
   public:
    void Resolve() {
      base::AutoLock auto_lock(lock_);
      DCHECK(!resolved_);
      resolved_ = true;

      if (!task_runner_) {
        return;
      }

      for (auto& response : response_closures_)
        task_runner_->PostTask(FROM_HERE, std::move(response));

      response_closures_.clear();
    }

    void Add(base::OnceClosure response) {
      base::AutoLock auto_lock(lock_);

      if (resolved_) {
        std::move(response).Run();
        return;
      }

      scoped_refptr<base::SingleThreadTaskRunner> task_runner =
          base::SingleThreadTaskRunner::GetCurrentDefault();
      if (task_runner_) {
        DCHECK_EQ(task_runner_, task_runner);
      } else {
        task_runner_ = std::move(task_runner);
      }

      response_closures_.push_back(std::move(response));
    }

   private:
    friend class base::RefCountedThreadSafe<Resolver>;
    ~Resolver() = default;

    base::Lock lock_;

    std::vector<base::OnceClosure> response_closures_;
    bool resolved_ GUARDED_BY(lock_) = false;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_ GUARDED_BY(lock_);
  };

  explicit DelayedHttpResponseWithResolver(scoped_refptr<Resolver> resolver)
      : resolver_(std::move(resolver)) {}

  DelayedHttpResponseWithResolver(const DelayedHttpResponseWithResolver&) =
      delete;
  DelayedHttpResponseWithResolver& operator=(
      const DelayedHttpResponseWithResolver&) = delete;

  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    resolver_->Add(base::BindOnce(
        &net::test_server::HttpResponseDelegate::SendHeadersContentAndFinish,
        delegate, code(), GetHttpReasonPhrase(code()), BuildHeaders(),
        content()));
  }

 private:
  const scoped_refptr<Resolver> resolver_;
};

std::unique_ptr<net::test_server::HttpResponse> HandleBeacon(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/beacon")
    return nullptr;
  return std::make_unique<net::test_server::BasicHttpResponse>();
}

std::unique_ptr<net::test_server::HttpResponse> HandleHungBeacon(
    const base::RepeatingClosure& on_called,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/beacon")
    return nullptr;
  if (on_called) {
    on_called.Run();
  }
  return std::make_unique<net::test_server::HungResponse>();
}

std::unique_ptr<net::test_server::HttpResponse> HandleHungBeaconWithResolver(
    scoped_refptr<DelayedHttpResponseWithResolver::Resolver> resolver,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/beacon")
    return nullptr;
  return std::make_unique<DelayedHttpResponseWithResolver>(std::move(resolver));
}

}  // namespace

class RenderProcessHostTestBase : public ContentBrowserTest,
                                  public RenderProcessHostObserver {
 public:
  RenderProcessHostTestBase() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetVisibleClients(RenderProcessHost* process, int32_t visible_clients) {
    RenderProcessHostImpl* impl = static_cast<RenderProcessHostImpl*>(process);
    impl->visible_clients_ = visible_clients;
    impl->UpdateProcessPriority();
  }

 protected:
  void SetProcessExitCallback(RenderProcessHost* rph,
                              base::OnceClosure callback) {
    Observe(rph);
    process_exit_callback_ = std::move(callback);
  }

  void Observe(RenderProcessHost* rph) {
    DCHECK(!observation_.IsObserving());
    observation_.Observe(rph);
  }

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    ++process_exits_;
    if (process_exit_callback_)
      std::move(process_exit_callback_).Run();
  }
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    ++host_destructions_;
    observation_.Reset();
  }
  void WaitUntilProcessExits(int target) {
    while (process_exits_ < target)
      base::RunLoop().RunUntilIdle();
  }

  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      observation_{this};
  int process_exits_ = 0;
  int host_destructions_ = 0;
  base::OnceClosure process_exit_callback_;
};

// A ContentBrowserClient that can wait for calls to
// `blink::mojom::KeepAliveHandle`.
class KeepAliveHandleContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit KeepAliveHandleContentBrowserClient(base::OnceClosure callback) {
    started_callback_ = std::move(callback);
  }
  void OnKeepaliveRequestStarted(BrowserContext* browser_context) override {
    ContentBrowserTestContentBrowserClient::OnKeepaliveRequestStarted(
        browser_context);
    CHECK(started_callback_);
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        std::move(started_callback_));
  }

 private:
  base::OnceClosure started_callback_;
};

class RenderProcessHostTest : public RenderProcessHostTestBase,
                              public ::testing::WithParamInterface<bool> {
 public:
  RenderProcessHostTest() = default;

  void SetUp() override {
    if (IsKeepAliveInBrowserMigrationEnabled()) {
      feature_list_.InitAndEnableFeature(
          blink::features::kKeepAliveInBrowserMigration);
    } else {
      feature_list_.InitAndDisableFeature(
          blink::features::kKeepAliveInBrowserMigration);
    }
    RenderProcessHostTestBase::SetUp();
  }

 protected:
  bool IsKeepAliveInBrowserMigrationEnabled() { return GetParam(); }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    RenderProcessHostTest,
    testing::Values(false, true),
    [](const testing::TestParamInfo<RenderProcessHostTest::ParamType>& info) {
      return info.param ? "KeepAliveInBrowserMigration" : "Default";
    });

IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, GuestsAreNotSuitableHosts) {
  // Set max renderers to 1 to force running out of processes.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  // Make it believe it's a guest.
  static_cast<RenderProcessHostImpl*>(rph)->SetForGuestsOnlyForTesting();
  EXPECT_EQ(1, RenderProcessHost::GetCurrentRenderProcessCountForTesting());

  // Navigate to a different page.
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  GURL another_url = embedded_test_server()->GetURL("/simple_page.html");
  another_url = another_url.ReplaceComponents(replace_host);
  EXPECT_TRUE(NavigateToURL(CreateBrowser(), another_url));

  // Expect that we got another process (the guest renderer was not reused).
  EXPECT_EQ(2, RenderProcessHost::GetCurrentRenderProcessCountForTesting());
}

class ShellCloser : public RenderProcessHostObserver {
 public:
  ShellCloser(Shell* shell, std::string* logging_string)
      : shell_(shell), logging_string_(logging_string) {}

 protected:
  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    logging_string_->append("ShellCloser::RenderProcessExited ");
    shell_->Close();
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    logging_string_->append("ShellCloser::RenderProcessHostDestroyed ");
  }

  raw_ptr<Shell, AcrossTasksDanglingUntriaged> shell_;
  raw_ptr<std::string> logging_string_;
};

class ObserverLogger : public RenderProcessHostObserver {
 public:
  explicit ObserverLogger(std::string* logging_string)
      : logging_string_(logging_string), host_destroyed_(false) {}

  bool host_destroyed() { return host_destroyed_; }

 protected:
  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    logging_string_->append("ObserverLogger::RenderProcessExited ");
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    logging_string_->append("ObserverLogger::RenderProcessHostDestroyed ");
    host_destroyed_ = true;
  }

  raw_ptr<std::string> logging_string_;
  bool host_destroyed_;
};

// Flaky on Android. http://crbug.com/759514.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_AllProcessExitedCallsBeforeAnyHostDestroyedCalls \
  DISABLED_AllProcessExitedCallsBeforeAnyHostDestroyedCalls
#else
#define MAYBE_AllProcessExitedCallsBeforeAnyHostDestroyedCalls \
  AllProcessExitedCallsBeforeAnyHostDestroyedCalls
#endif
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       MAYBE_AllProcessExitedCallsBeforeAnyHostDestroyedCalls) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  std::string logging_string;
  ShellCloser shell_closer(shell(), &logging_string);
  ObserverLogger observer_logger(&logging_string);
  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // Ensure that the ShellCloser observer is first, so that it will have first
  // dibs on the ProcessExited callback.
  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      observation_1(&shell_closer);
  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      observation_2(&observer_logger);
  observation_1.Observe(rph);
  observation_2.Observe(rph);

  // This will crash the render process, and start all the callbacks.
  // We can't use NavigateToURL here since it accesses the shell() after
  // navigating, which the shell_closer deletes.
  ScopedAllowRendererCrashes scoped_allow_renderer_crashes(shell());
  NavigateToURLBlockUntilNavigationsComplete(shell(),
                                             GURL(blink::kChromeUICrashURL), 1);

  // The key here is that all the RenderProcessExited callbacks precede all the
  // RenderProcessHostDestroyed callbacks.
  EXPECT_EQ(
      "ShellCloser::RenderProcessExited "
      "ObserverLogger::RenderProcessExited "
      "ShellCloser::RenderProcessHostDestroyed "
      "ObserverLogger::RenderProcessHostDestroyed ",
      logging_string);
}

IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, KillProcessOnBadMojoMessage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  host_destructions_ = 0;
  process_exits_ = 0;

  mojo::Remote<mojom::TestService> service;
  rph->BindReceiver(service.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  SetProcessExitCallback(rph, run_loop.QuitClosure());

  // Should reply with a bad message and cause process death.
  {
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(rph);
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
}

// Observes a WebContents and a specific frame within it, and waits until they
// both indicate that they are audible.
class AudioStartObserver : public WebContentsObserver {
 public:
  AudioStartObserver(WebContents* web_contents,
                     RenderFrameHost* render_frame_host,
                     base::OnceClosure audible_closure)
      : WebContentsObserver(web_contents),
        render_frame_host_(
            static_cast<RenderFrameHostImpl*>(render_frame_host)),
        contents_audible_(web_contents->IsCurrentlyAudible()),
        frame_audible_(render_frame_host_->HasMediaStreams(
            RenderFrameHostImpl::GetAudibleMediaStreamType())),
        audible_closure_(std::move(audible_closure)) {
    MaybeFireClosure();
  }
  ~AudioStartObserver() override = default;

  // WebContentsObserver:
  void OnAudioStateChanged(bool audible) override {
    DCHECK_NE(audible, contents_audible_);
    contents_audible_ = audible;
    MaybeFireClosure();
  }
  void OnFrameAudioStateChanged(RenderFrameHost* render_frame_host,
                                bool audible) override {
    if (render_frame_host_ != render_frame_host)
      return;
    DCHECK_NE(frame_audible_, audible);
    frame_audible_ = audible;
    MaybeFireClosure();
  }

 private:
  void MaybeFireClosure() {
    if (contents_audible_ && frame_audible_)
      std::move(audible_closure_).Run();
  }

  raw_ptr<RenderFrameHostImpl> render_frame_host_ = nullptr;
  bool contents_audible_ = false;
  bool frame_audible_ = false;
  base::OnceClosure audible_closure_;
};

// Tests that audio stream counts (used for process priority calculations) are
// properly set and cleared during media playback and renderer terminations.
//
// Note: This test can't run when the Mojo Renderer is used since it does not
// create audio streams through the normal audio pathways; at present this is
// only used by Chromecast.
//
// crbug.com/864476: flaky on Android for unclear reasons.
#if BUILDFLAG(ENABLE_MOJO_RENDERER) || BUILDFLAG(IS_ANDROID)
#define KillProcessZerosAudioStreams DISABLED_KillProcessZerosAudioStreams
#endif
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, KillProcessZerosAudioStreams) {
  // TODO(maxmorin): This test only uses an output stream. There should be a
  // similar test for input streams.
  embedded_test_server()->ServeFilesFromSourceDirectory(
      media::GetTestDataPath());
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/webaudio_oscillator.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());

  {
    // Start audio and wait for it to become audible, in both the frame *and*
    // the page.
    base::RunLoop run_loop;
    AudioStartObserver observer(shell()->web_contents(),
                                shell()->web_contents()->GetPrimaryMainFrame(),
                                run_loop.QuitClosure());

    std::string result;
    EXPECT_EQ("OK", EvalJs(shell(), "StartOscillator();"))
        << "Failed to execute javascript.";
    run_loop.Run();

    // No point in running the rest of the test if this is wrong.
    ASSERT_EQ(1, rph->get_media_stream_count_for_testing());
  }

  host_destructions_ = 0;
  process_exits_ = 0;

  mojo::Remote<mojom::TestService> service;
  rph->BindReceiver(service.BindNewPipeAndPassReceiver());

  {
    // Force a bad message event to occur which will terminate the renderer.
    // Note: We post task the QuitClosure since RenderProcessExited() is called
    // before destroying BrowserMessageFilters; and the next portion of the test
    // must run after these notifications have been delivered.
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(rph);
    base::RunLoop run_loop;
    SetProcessExitCallback(rph, run_loop.QuitClosure());
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  {
    // Cycle UI and IO loop once to ensure OnChannelClosing() has been delivered
    // to audio stream owners and they get a chance to notify of stream closure.
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindPostTaskToCurrentDefault(run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Verify shutdown went as expected.
  EXPECT_EQ(0, rph->get_media_stream_count_for_testing());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
}

// Test class instance to run specific setup steps for capture streams.
class CaptureStreamRenderProcessHostTest : public RenderProcessHostTestBase {
 public:
  void SetUp() override {
    // Pixel output is needed when digging pixel values out of video tags for
    // verification in VideoCaptureStream tests.
    EnablePixelOutput();
    RenderProcessHostTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // These flags are necessary to emulate camera input for getUserMedia()
    // tests.
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    RenderProcessHostTestBase::SetUpCommandLine(command_line);
  }
};

// Tests that video capture stream count increments when getUserMedia() is
// called.
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       GetUserMediaIncrementsVideoCaptureStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAndExpectSuccess({video: true});"))
      << "Failed to execute javascript.";
  EXPECT_EQ(1, rph->get_media_stream_count_for_testing());
}

// Tests that video capture stream count resets when getUserMedia() is called
// and stopped.
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       StopResetsVideoCaptureStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAndStop({video: true});"))
      << "Failed to execute javascript.";
  EXPECT_EQ(0, rph->get_media_stream_count_for_testing());
}

// Tests that video capture stream counts (used for process priority
// calculations) are properly set and cleared during media playback and renderer
// terminations.
// Test is flaky on Android builders: https://crbug.com/352065578
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_KillProcessZerosVideoCaptureStreams \
  DISABLED_KillProcessZerosVideoCaptureStreams
#else
#define MAYBE_KillProcessZerosVideoCaptureStreams \
  KillProcessZerosVideoCaptureStreams
#endif
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       MAYBE_KillProcessZerosVideoCaptureStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecJs(shell(), "getUserMediaAndExpectSuccess({video: true});"))
      << "Failed to execute javascript.";
  EXPECT_EQ(1, rph->get_media_stream_count_for_testing());

  host_destructions_ = 0;
  process_exits_ = 0;

  mojo::Remote<mojom::TestService> service;
  rph->BindReceiver(service.BindNewPipeAndPassReceiver());

  {
    // Force a bad message event to occur which will terminate the renderer.
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(rph);
    base::RunLoop run_loop;
    SetProcessExitCallback(rph, run_loop.QuitClosure());
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  {
    // Cycle UI and IO loop once to ensure OnChannelClosing() has been delivered
    // to audio stream owners and they get a chance to notify of stream closure.
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindPostTaskToCurrentDefault(run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(0, rph->get_media_stream_count_for_testing());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
}

// Tests that media stream count increments when getUserMedia() is
// called with audio only.
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       GetUserMediaAudioOnlyIncrementsMediaStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecJs(
      shell(), "getUserMediaAndExpectSuccess({video: false, audio: true});"))
      << "Failed to execute javascript.";
  EXPECT_EQ(1, rph->get_media_stream_count_for_testing());
}

// Tests that media stream counts (used for process priority
// calculations) are properly set and cleared during media playback and renderer
// terminations for audio only streams.
// Test is flaky on Android builders: https://crbug.com/352065578
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_KillProcessZerosVideoCaptureStreams \
  DISABLED_KillProcessZerosVideoCaptureStreams
#else
#define MAYBE_KillProcessZerosVideoCaptureStreams \
  KillProcessZerosVideoCaptureStreams
#endif
IN_PROC_BROWSER_TEST_F(CaptureStreamRenderProcessHostTest,
                       KillProcessZerosAudioCaptureStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/media/getusermedia.html")));
  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  std::string result;
  EXPECT_TRUE(ExecJs(
      shell(), "getUserMediaAndExpectSuccess({video: false, audio: true});"))
      << "Failed to execute javascript.";
  EXPECT_EQ(1, rph->get_media_stream_count_for_testing());

  host_destructions_ = 0;
  process_exits_ = 0;

  mojo::Remote<mojom::TestService> service;
  rph->BindReceiver(service.BindNewPipeAndPassReceiver());

  {
    // Force a bad message event to occur which will terminate the renderer.
    ScopedAllowRendererCrashes scoped_allow_renderer_crashes(rph);
    base::RunLoop run_loop;
    SetProcessExitCallback(rph, run_loop.QuitClosure());
    service->DoSomething(base::DoNothing());
    run_loop.Run();
  }

  {
    // Cycle UI and IO loop once to ensure OnChannelClosing() has been delivered
    // to audio stream owners and they get a chance to notify of stream closure.
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindPostTaskToCurrentDefault(run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(0, rph->get_media_stream_count_for_testing());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
}

IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, KeepAliveRendererProcess) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleBeacon));
  ASSERT_TRUE(embedded_test_server()->Start());

  if (AreDefaultSiteInstancesEnabled()) {
    // Isolate "foo.com" so we are guaranteed that navigations to this site
    // will be in a different process.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {"foo.com"});
  }

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/send-beacon.html")));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(rfh->GetProcess());

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  host_destructions_ = 0;
  process_exits_ = 0;
  Observe(rph);
  rfh->SetKeepAliveTimeoutForTesting(base::Seconds(30));

  if (IsKeepAliveInBrowserMigrationEnabled()) {
    // When fetch keepalive in browser migration is enabled, the process will be
    // able to exit immediately. Instead of verifying the time it takes until it
    // exits, verify that the `keep_alive_ref_count()` is as expected.
    EXPECT_EQ(rph->keep_alive_ref_count(), 0);
  }

  // Navigate to a site that will be in a different process.
  base::TimeTicks start = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));

  WaitUntilProcessExits(1);

  EXPECT_LT(base::TimeTicks::Now() - start, base::Seconds(30));
}

// TODO(crbug.com/40275040): Fix and re-enable.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       DISABLED_KeepAliveRendererProcessWithServiceWorker) {
  if (IsKeepAliveInBrowserMigrationEnabled()) {
    // TODO(crbug.com/40236167): Add keepalive in-browser support for workers.
    return;
  }

  base::RunLoop run_loop;
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, run_loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/workers/service_worker_setup.html")));
  EXPECT_EQ("ok", EvalJs(shell(), "setup();"));

  RenderProcessHostImpl* rph = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  // 1 for the service worker.
  EXPECT_EQ(rph->worker_ref_count(), 1);
  EXPECT_EQ(rph->keep_alive_ref_count(), 0);

  // We use /workers/send-beacon.html, not send-beacon.html, due to the
  // service worker scope rule.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/workers/send-beacon.html")));

  run_loop.Run();
  // We are still using the same process.
  ASSERT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(), rph);
  // 1 for the service worker, 1 for the keepalive fetch.
  if (!IsKeepAliveInBrowserMigrationEnabled()) {
    EXPECT_EQ(rph->keep_alive_ref_count(), 1);
  } else {
    // When fetch keepalive in browser migration is enabled, the process will be
    // able to exit immediately.
    EXPECT_EQ(rph->keep_alive_ref_count(), 0);
  }
  EXPECT_EQ(rph->worker_ref_count(), 1);
}

// Test is flaky on Android builders: https://crbug.com/875179
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_KeepAliveRendererProcess_Hung \
  DISABLED_KeepAliveRendererProcess_Hung
#else
#define MAYBE_KeepAliveRendererProcess_Hung KeepAliveRendererProcess_Hung
#endif
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       MAYBE_KeepAliveRendererProcess_Hung) {
  // Disable HangWatcher so it doesn't interfere with this test when hangs take
  // place.
  base::HangWatcher::StopMonitoringForTesting();

  // The test assumes that the render process exits after 1 second. But this
  // will be prevented if the process still hosts a bfcached page. So disable
  // BFCache for this test.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, base::RepeatingClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  const auto kTestUrl = embedded_test_server()->GetURL("/send-beacon.html");
  if (IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
    // Isolate host so that the first and second navigation are guaranteed to
    // be in different processes.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {kTestUrl.host()});
  }
  EXPECT_TRUE(NavigateToURL(shell(), kTestUrl));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(rfh->GetProcess());

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  host_destructions_ = 0;
  process_exits_ = 0;
  Observe(rph);
  rfh->SetKeepAliveTimeoutForTesting(base::Seconds(1));

  if (IsKeepAliveInBrowserMigrationEnabled()) {
    // When fetch keepalive in browser migration is enabled, the process will be
    // able to exit immediately. Instead of verifying the time it takes until it
    // exits, verify that the `keep_alive_ref_count()` is as expected.
    EXPECT_EQ(rph->keep_alive_ref_count(), 0);
  }

  base::TimeTicks start = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,<p>hello</p>")));

  WaitUntilProcessExits(1);

  if (!IsKeepAliveInBrowserMigrationEnabled()) {
    EXPECT_GE(base::TimeTicks::Now() - start, base::Seconds(1));
  }
}

// Test is flaky on Android builders: https://crbug.com/875179
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_FetchKeepAliveRendererProcess_Hung \
  DISABLED_FetchKeepAliveRendererProcess_Hung
#else
#define MAYBE_FetchKeepAliveRendererProcess_Hung \
  FetchKeepAliveRendererProcess_Hung
#endif
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       MAYBE_FetchKeepAliveRendererProcess_Hung) {
  // Disable HangWatcher so it doesn't interfere with this test when hangs take
  // place.
  base::HangWatcher::StopMonitoringForTesting();

  // The test assumes that the render process exits after 1 second. But this
  // will be prevented if the process still hosts a bfcached page. So disable
  // BFCache for this test.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, base::RepeatingClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  const auto kTestUrl = embedded_test_server()->GetURL("/fetch-keepalive.html");
  if (IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
    // Isolate host so that the first and second navigation are guaranteed to
    // be in different processes.
    IsolateOriginsForTesting(embedded_test_server(), shell()->web_contents(),
                             {kTestUrl.host()});
  }

  EXPECT_TRUE(NavigateToURL(shell(), kTestUrl));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(rfh->GetProcess());

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(shell()->web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  host_destructions_ = 0;
  process_exits_ = 0;
  Observe(rph);
  rfh->SetKeepAliveTimeoutForTesting(base::Seconds(1));

  if (IsKeepAliveInBrowserMigrationEnabled()) {
    // Wait for the page to make the keepalive request.
    const std::u16string waiting = u"Waiting";
    TitleWatcher watcher(shell()->web_contents(), waiting);
    ASSERT_EQ(waiting, watcher.WaitAndGetTitle());
    // When fetch keepalive in browser migration is enabled, the process will be
    // able to exit immediately. Instead of verifying the time it takes until it
    // exits, verify that the `keep_alive_ref_count()` is as expected.
    EXPECT_EQ(rph->keep_alive_ref_count(), 0);
  }

  base::TimeTicks start = base::TimeTicks::Now();
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,<p>hello</p>")));

  WaitUntilProcessExits(1);

  if (!IsKeepAliveInBrowserMigrationEnabled()) {
    EXPECT_GE(base::TimeTicks::Now() - start, base::Seconds(1));
  }
}

IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, ManyKeepaliveRequests) {
  auto resolver =
      base::MakeRefCounted<DelayedHttpResponseWithResolver::Resolver>();
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeaconWithResolver, resolver));
  const std::u16string title = u"Resolved";
  const std::u16string waiting = u"Waiting";

  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/fetch-keepalive.html?requests=256")));

  {
    // Wait for the page to make all the keepalive requests.
    TitleWatcher watcher(shell()->web_contents(), waiting);
    EXPECT_EQ(waiting, watcher.WaitAndGetTitle());
  }

  resolver->Resolve();

  {
    TitleWatcher watcher(shell()->web_contents(), title);
    EXPECT_EQ(title, watcher.WaitAndGetTitle());
  }
}

IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, TooManyKeepaliveRequests) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, base::RepeatingClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  const std::u16string title = u"Rejected";

  TitleWatcher watcher(shell()->web_contents(), title);

  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/fetch-keepalive.html?requests=257")));

  EXPECT_EQ(title, watcher.WaitAndGetTitle());
}

// Records the value of |host->IsProcessBackgrounded()| when it changes.
// |host| must remain a valid reference for the lifetime of this object.
class IsProcessBackgroundedObserver : public RenderProcessHostInternalObserver {
 public:
  explicit IsProcessBackgroundedObserver(RenderProcessHostImpl* host) {
    host_observation_.Observe(host);
  }

  void RenderProcessPriorityChanged(RenderProcessHostImpl* host) override {
    priority_ = host->GetPriority();
  }

  // Returns the latest recorded value if there was one and resets the recorded
  // value to |nullopt|.
  std::optional<base::Process::Priority> TakeValue() {
    auto value = priority_;
    priority_ = std::nullopt;
    return value;
  }

 private:
  // Stores the last observed value of GetPriority() for a host.
  std::optional<base::Process::Priority> priority_;
  base::ScopedObservation<RenderProcessHostImpl,
                          RenderProcessHostInternalObserver>
      host_observation_{this};
};

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, PriorityOverride) {
  // Start up a real renderer process.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostImpl* process = static_cast<RenderProcessHostImpl*>(rph);

  IsProcessBackgroundedObserver observer(process);

  // It starts off as normal priority with no override.
  EXPECT_FALSE(process->HasPriorityOverride());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserBlocking);
  EXPECT_FALSE(observer.TakeValue().has_value());

  process->SetPriorityOverride(base::Process::Priority::kBestEffort);
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kBestEffort);
  EXPECT_EQ(observer.TakeValue().value(), process->GetPriority());

  process->SetPriorityOverride(base::Process::Priority::kUserBlocking);
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserBlocking);
  EXPECT_EQ(observer.TakeValue().value(), process->GetPriority());

  process->SetPriorityOverride(base::Process::Priority::kBestEffort);
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kBestEffort);
  EXPECT_EQ(observer.TakeValue().value(), process->GetPriority());

  // Add a media stream, and expect the process to *stay* backgrounded.
  process->OnMediaStreamAdded();
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kBestEffort);
  EXPECT_EQ(observer.TakeValue().value(), process->GetPriority());

  // TODO(pmonette): Pending views will be taken into account if
  // kPriorityOverridePendingViews is enabled.
  base::Process::Priority kExpectedPriorityPendingViews =
      base::FeatureList::IsEnabled(features::kPriorityOverridePendingViews)
          ? base::Process::Priority::kUserBlocking
          : base::Process::Priority::kBestEffort;

  process->AddPendingView();
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_EQ(process->GetPriority(), kExpectedPriorityPendingViews);
  EXPECT_EQ(observer.TakeValue().value(), process->GetPriority());

  process->RemovePendingView();
  EXPECT_TRUE(process->HasPriorityOverride());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kBestEffort);
  EXPECT_EQ(observer.TakeValue().value(), process->GetPriority());

  // Clear the override. The media stream should cause the process to go back to
  // being foregrounded.
  process->ClearPriorityOverride();
  EXPECT_FALSE(process->HasPriorityOverride());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserBlocking);
  EXPECT_EQ(observer.TakeValue().value(), process->GetPriority());

  // Clear the media stream so the test doesn't explode.
  process->OnMediaStreamRemoved();
}
#endif  // !BUILDFLAG(IS_ANDROID)

struct BoostRenderProcessForLoadingBrowserTestParam {
  bool enable_boost_render_process_for_loading;
  std::string target_urls;
  bool renderer_initiated_navigation;
  bool expect_render_process_backgrounded;
};

// This test verifies `kBoostRenderProcessForLoading` feature can keep the
// RenderProcessHost foregrounded until `DOMContentLoaded` comes.
class BoostRenderProcessForLoadingBrowserTest
    : public RenderProcessHostTestBase,
      public content::WebContentsObserver,
      public ::testing::WithParamInterface<
          BoostRenderProcessForLoadingBrowserTestParam> {
 public:
  BoostRenderProcessForLoadingBrowserTest() {
    if (GetParam().enable_boost_render_process_for_loading) {
      feature_list_.InitWithFeaturesAndParameters(
          {{blink::features::kBoostRenderProcessForLoading,
            {{blink::features::kBoostRenderProcessForLoadingTargetUrls.name,
              GetParam().target_urls},
             {"prioritize_renderer_initiated", "false"}}}},
          {});
    } else {
      feature_list_.InitAndDisableFeature(
          blink::features::kBoostRenderProcessForLoading);
    }
  }

  void SetUpOnMainThread() override {
    content::WebContentsObserver::Observe(&web_contents());
    RenderProcessHostTestBase::SetUpOnMainThread();
  }

  content::WebContents& web_contents() { return *shell()->web_contents(); }

 private:
  // content::WebContentsObserver:
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override {
    if (!check_if_render_process_backgrounded_on_dom_content_loaded_) {
      return;
    }
    RenderProcessHost* render_process_host = render_frame_host->GetProcess();
    // Emulate render_process_host is not visible to users.
    SetVisibleClients(render_process_host, 0);
    EXPECT_EQ(render_process_host->GetPriority() ==
                  base::Process::Priority::kBestEffort,
              GetParam().expect_render_process_backgrounded);
  }

 protected:
  bool check_if_render_process_backgrounded_on_dom_content_loaded_ = false;
  base::test::ScopedFeatureList feature_list_;
};

const BoostRenderProcessForLoadingBrowserTestParam
    kBoostRenderProcessForLoadingBrowserTestParams[] = {
        {
            .enable_boost_render_process_for_loading = false,
            .target_urls = "[]",
            .renderer_initiated_navigation = false,
            .expect_render_process_backgrounded = true,
        },
        {
            .enable_boost_render_process_for_loading = true,
            .target_urls = "[]",
            .renderer_initiated_navigation = false,
            .expect_render_process_backgrounded = false,
        },
        {
            .enable_boost_render_process_for_loading = true,
            .target_urls = "[]",
            .renderer_initiated_navigation = true,
            .expect_render_process_backgrounded = true,
        },
        {
            .enable_boost_render_process_for_loading = true,
            .target_urls = "[\"http://a.com/simple_page.html\", "
                           "\"http://b.com/simple_page.html\"]",
            .renderer_initiated_navigation = false,
            .expect_render_process_backgrounded = false,
        },
        {
            .enable_boost_render_process_for_loading = true,
            .target_urls = "[\"http://b.com/simple_page.html\", "
                           "\"http://c.com/simple_page.html\"]",
            .renderer_initiated_navigation = false,
            .expect_render_process_backgrounded = true,
        },
        {
            .enable_boost_render_process_for_loading = true,
            .target_urls = "[\"http://a.co.jp/simple_page.html\"]",
            .renderer_initiated_navigation = false,
            .expect_render_process_backgrounded = false,
        },
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BoostRenderProcessForLoadingBrowserTest,
    testing::ValuesIn(kBoostRenderProcessForLoadingBrowserTestParams));

IN_PROC_BROWSER_TEST_P(BoostRenderProcessForLoadingBrowserTest,
                       VerifyRenderProcessBackgrounded) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL empty_url(embedded_test_server()->GetURL("a.com", "/empty.html"));
  GURL test_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));

  EXPECT_TRUE(NavigateToURL(shell(), empty_url));

  // `BoostRenderProcessForLoadingBrowserTest::DOMContentLoaded()` will be
  // called during `NavigateToURL()` to check the renderer process priority.
  check_if_render_process_backgrounded_on_dom_content_loaded_ = true;
  if (GetParam().renderer_initiated_navigation) {
    EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1", test_url)));
    EXPECT_TRUE(WaitForLoadStop(&web_contents()));
  } else {
    EXPECT_TRUE(NavigateToURL(shell(), test_url));
  }

  // Emulate render_process_host is not visible to users.
  RenderProcessHost* render_process_host =
      web_contents().GetPrimaryMainFrame()->GetProcess();
  SetVisibleClients(render_process_host, 0);
  EXPECT_EQ(render_process_host->GetPriority(),
            base::Process::Priority::kBestEffort);
}

// This test verifies properties of RenderProcessHostImpl *before* Init method
// is called.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, ConstructedButNotInitializedYet) {
  RenderProcessHost* process = RenderProcessHostImpl::CreateRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context(), nullptr);

  // Just verifying that the arguments of CreateRenderProcessHost got processed
  // correctly.
  EXPECT_EQ(ShellContentBrowserClient::Get()->browser_context(),
            process->GetBrowserContext());
  EXPECT_FALSE(process->IsForGuestsOnly());

  // There should be no OS process before Init() method is called.
  EXPECT_FALSE(process->IsInitializedAndNotDead());
  EXPECT_FALSE(process->IsReady());
  EXPECT_FALSE(process->GetProcess().IsValid());
  EXPECT_EQ(base::kNullProcessHandle, process->GetProcess().Handle());

  // TODO(lukasza): https://crbug.com/813045: RenderProcessHost shouldn't have
  // an associated IPC channel (and shouldn't accumulate IPC messages) unless
  // the Init() method was called and the RPH either has connection to an actual
  // OS process or is currently attempting to spawn the OS process.  After this
  // bug is fixed the 1st test assertion below should be reversed (unsure about
  // the 2nd one).
  EXPECT_TRUE(process->GetChannel());
  EXPECT_TRUE(process->GetRendererInterface());

  // Cleanup the resources acquired by the test.
  process->Cleanup();
}

// This test verifies that a fast shutdown is possible for a starting process.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, FastShutdownForStartingProcess) {
  RenderProcessHost* process = RenderProcessHostImpl::CreateRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context(), nullptr);
  process->Init();
  EXPECT_TRUE(process->FastShutdownIfPossible());
  process->Cleanup();
}

// Verifies that a fast shutdown is possible with pending keepalive request.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       FastShutdownWithKeepAliveRequest) {
  base::RunLoop request_sent_loop, request_handled_loop;
  KeepAliveHandleContentBrowserClient browser_client(
      request_handled_loop.QuitClosure());
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(HandleHungBeacon, request_sent_loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  const auto kTestUrl = embedded_test_server()->GetURL("/send-beacon.html");
  ASSERT_TRUE(NavigateToURL(shell(), kTestUrl));
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  RenderProcessHostImpl* rph =
      static_cast<RenderProcessHostImpl*>(rfh->GetProcess());
  // Ensure keepalive request is sent.
  request_sent_loop.Run();

  if (IsKeepAliveInBrowserMigrationEnabled()) {
    // When fetch keepalive in browser migration is enabled, the process will be
    // able to exit immediately. Verify that the `keep_alive_ref_count()` is as
    // expected.
    EXPECT_EQ(rph->keep_alive_ref_count(), 0);
    EXPECT_TRUE(rph->FastShutdownIfPossible());
  } else {
    request_handled_loop.Run();
    EXPECT_EQ(rph->keep_alive_ref_count(), 1);
    EXPECT_FALSE(rph->FastShutdownIfPossible());
  }
}

// Tests that all RenderFrameHosts that lives in the process are accessible via
// RenderProcessHost::ForEachRenderFrameHost(), except those RenderFrameHosts
// whose lifecycle states are kSpeculative.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, ForEachRenderFrameHost) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(a,a))");
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title1.html");
  GURL url_new_window = embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(a)");
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // 1. Navigate to `url_a`; it creates 4 RenderFrameHosts that live in the
  // same process.
  ASSERT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHost* rfh_a = shell()->web_contents()->GetPrimaryMainFrame();
  std::vector<RenderFrameHost*> process_a_frames =
      CollectAllRenderFrameHosts(rfh_a);

  // 2. Open a new window, and navigate to a page with an a.com subframe.
  // The new subframe should reuse the same a.com process.
  Shell* new_window = CreateBrowser();
  ASSERT_TRUE(NavigateToURL(new_window, url_new_window));
  auto* new_window_main_frame = static_cast<RenderFrameHostImpl*>(
      new_window->web_contents()->GetPrimaryMainFrame());
  ASSERT_EQ(new_window_main_frame->child_count(), 1u);
  RenderFrameHost* new_window_sub_frame =
      new_window_main_frame->child_at(0)->current_frame_host();
  ASSERT_EQ(rfh_a->GetProcess(), new_window_sub_frame->GetProcess());
  process_a_frames.push_back(new_window_sub_frame);

  // 3. Start to navigate to a cross-origin site, and hold the navigation
  // request. This behavior will create a speculative RenderFrameHost.
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  TestNavigationManager manager(web_contents, url_b);
  shell()->LoadURL(url_b);
  manager.WaitForSpeculativeRenderFrameHostCreation();

  // 4. Get the speculative RenderFrameHost.
  FrameTreeNode* root = web_contents->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* rfh_b = root->render_manager()->speculative_frame_host();
  ASSERT_TRUE(rfh_b);
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kSpeculative,
            rfh_b->lifecycle_state());

  std::vector<RenderFrameHost*> same_process_rfhs;
  auto non_speculative_rfh_collector =
      [&same_process_rfhs](RenderFrameHost* rfh) {
        auto* rfhi = static_cast<RenderFrameHostImpl*>(rfh);
        EXPECT_NE(RenderFrameHostImpl::LifecycleStateImpl::kSpeculative,
                  rfhi->lifecycle_state());
        same_process_rfhs.push_back(rfh);
      };

  // 5. Check all of the a.com RenderFrameHosts are tracked by `rph_a`.
  RenderProcessHostImpl* rph_a =
      static_cast<RenderProcessHostImpl*>(rfh_a->GetProcess());
  rph_a->ForEachRenderFrameHost(non_speculative_rfh_collector);
  EXPECT_EQ(5, rph_a->GetRenderFrameHostCount());
  EXPECT_EQ(5u, same_process_rfhs.size());
  EXPECT_THAT(same_process_rfhs,
              testing::UnorderedElementsAreArray(process_a_frames.data(),
                                                 process_a_frames.size()));

  // 6. Check the speculative RenderFrameHost is ignored.
  same_process_rfhs.clear();
  RenderProcessHostImpl* rph_b =
      static_cast<RenderProcessHostImpl*>(rfh_b->GetProcess());
  ASSERT_NE(rph_a, rph_b);
  rph_b->ForEachRenderFrameHost(non_speculative_rfh_collector);
  EXPECT_EQ(1, rph_b->GetRenderFrameHostCount());
  // The speculative RenderFrameHost should be filtered out.
  EXPECT_EQ(same_process_rfhs.size(), 0u);

  // 7. Resume the blocked navigation.
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  // 8. Check that `RenderProcessHost::ForEachRenderFrameHost` does not filter
  // `rfh_b` out, because its lifecycle has changed to kActive.
  EXPECT_EQ(RenderFrameHostImpl::LifecycleStateImpl::kActive,
            rfh_b->lifecycle_state());

  EXPECT_EQ(1, rph_b->GetRenderFrameHostCount());
  same_process_rfhs.clear();
  rph_b->ForEachRenderFrameHost(non_speculative_rfh_collector);
  EXPECT_EQ(1, rph_b->GetRenderFrameHostCount());
  EXPECT_EQ(1u, same_process_rfhs.size());
  EXPECT_THAT(same_process_rfhs, testing::ElementsAre(rfh_b));
}

namespace {

// Observer that listens for process leak cleanup events. Note that this only
// hears about cases where the affected RenderViewHost is for the primary main
// frame.
class LeakCleanupObserver : public WebContentsObserver {
 public:
  explicit LeakCleanupObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~LeakCleanupObserver() override = default;

  // WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    termination_count_++;
    CHECK_EQ(status,
             base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION);
  }

  int termination_count() { return termination_count_; }

  void reset_termination_count() { termination_count_ = 0; }

 private:
  int termination_count_ = 0;
};

// Observer that listens for process exits and counts when they are treated as
// fast shutdown cases.
class FastShutdownExitObserver : public RenderProcessHostObserver {
 public:
  explicit FastShutdownExitObserver(RenderProcessHost* process) {
    process->AddObserver(this);
  }
  ~FastShutdownExitObserver() override = default;

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    if (host->FastShutdownStarted())
      fast_shutdown_exit_count_++;
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    host->RemoveObserver(this);
  }

  int fast_shutdown_exit_count() { return fast_shutdown_exit_count_; }

  void reset_exit_count() { fast_shutdown_exit_count_ = 0; }

 private:
  int fast_shutdown_exit_count_ = 0;
};

}  // namespace

// Ensure that we don't leak a renderer process if there are only non-live
// RenderFrameHosts assigned to its RenderProcessHost (e.g., when the last live
// frame goes away). See https://crbug.com/1226834.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, AllowUnusedProcessToExit) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure all sites get dedicated processes during the test.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Set max renderers to 1 to force reusing a renderer process between two
  // unrelated tabs.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // The test assumes that the render process exits after navigation, but this
  // will be prevented if the process still hosts a bfcached page. Disable
  // BFCache for this test.
  content::DisableBackForwardCacheForTesting(
      shell()->web_contents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Ensure the initial tab has not loaded yet.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* original_rfh = root->current_frame_host();
  RenderProcessHost* original_process = original_rfh->GetProcess();
  int original_process_id = original_process->GetID();
  EXPECT_FALSE(original_process->IsInitializedAndNotDead());
  EXPECT_FALSE(original_rfh->IsRenderFrameLive());

  // Reset the process exit related counts and listen to process exit events.
  process_exits_ = 0;
  host_destructions_ = 0;
  Observe(original_process);
  FastShutdownExitObserver fast_shutdown_observer(original_process);

  // Create a second tab to a real URL. This will share the first
  // RenderProcessHost due to the low limit, and because it was not used or
  // locked.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  Shell* shell2 =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             url_a, nullptr, gfx::Size());
  FrameTreeNode* root2 = static_cast<WebContentsImpl*>(shell2->web_contents())
                             ->GetPrimaryFrameTree()
                             .root();
  RenderFrameHostImpl* rfh2 = root2->current_frame_host();
  EXPECT_EQ(original_process, rfh2->GetProcess());
  EXPECT_TRUE(original_process->IsInitializedAndNotDead());
  EXPECT_TRUE(rfh2->IsRenderFrameLive());

  // The original RFH is still in an unloaded state.
  EXPECT_FALSE(original_rfh->IsRenderFrameLive());

  // Close shell2. This used to leave the process running, since original_rfh
  // was still counted as an active frame in the RenderProcessHost even though
  // it wasn't live.
  LeakCleanupObserver leak_cleanup_observer(shell()->web_contents());
  RenderProcessHostWatcher exit_observer(
      original_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  RenderFrameDeletedObserver rfh2_deleted_observer(rfh2);
  shell2->Close();
  rfh2_deleted_observer.WaitUntilDeleted();
  exit_observer.Wait();
  EXPECT_TRUE(exit_observer.did_exit_normally());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);
  EXPECT_EQ(1, leak_cleanup_observer.termination_count());

  // This cleanup should be considered similar to fast shutdown, for observers
  // that treat that case as expected behavior.
  EXPECT_EQ(1, fast_shutdown_observer.fast_shutdown_exit_count());

  EXPECT_EQ(original_rfh, root->current_frame_host());
  EXPECT_EQ(original_process, original_rfh->GetProcess());
  EXPECT_FALSE(original_process->IsInitializedAndNotDead());
  EXPECT_FALSE(original_rfh->IsRenderFrameLive());

  // There shouldn't be live RenderViewHosts or proxies either.
  EXPECT_FALSE(original_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(original_rfh->GetSiteInstance()->group()));

  // Reset the process exit related counts.
  process_exits_ = 0;
  host_destructions_ = 0;
  leak_cleanup_observer.reset_termination_count();
  fast_shutdown_observer.reset_exit_count();

  // After the leak cleanup, navigate the original frame to the same site to
  // make sure it still works in the original RenderProcessHost, even though it
  // swaps to a new RenderFrameHost.
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  RenderFrameHostImpl* replaced_rfh = root->current_frame_host();
  ASSERT_EQ(original_process, replaced_rfh->GetProcess());
  EXPECT_EQ(original_process_id, replaced_rfh->GetProcess()->GetID());
  EXPECT_TRUE(replaced_rfh->GetProcess()->IsInitializedAndNotDead());
  EXPECT_TRUE(replaced_rfh->IsRenderFrameLive());
  EXPECT_FALSE(original_process->FastShutdownStarted());
  EXPECT_EQ(0, process_exits_);
  EXPECT_EQ(0, host_destructions_);
  EXPECT_EQ(0, fast_shutdown_observer.fast_shutdown_exit_count());
  EXPECT_EQ(0, leak_cleanup_observer.termination_count());

  // Ensure that the leak cleanup does not occur after a normal cross-process
  // navigation, since normal process cleanup generates different events,
  // including the full destruction of the RenderProcessHost.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  RenderProcessHostWatcher cleanup_observer(
      original_process, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  cleanup_observer.Wait();
  RenderFrameHostImpl* rfh_b = root->current_frame_host();
  EXPECT_NE(original_process_id, rfh_b->GetProcess()->GetID());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(1, host_destructions_);

  // During the cross-process navigation, we should not have used the leak
  // cleanup approach. The leak cleanup would set FastShutdownStarted.
  EXPECT_EQ(0, fast_shutdown_observer.fast_shutdown_exit_count());
  EXPECT_EQ(0, leak_cleanup_observer.termination_count());
}

// Similar to AllowUnusedProcessToExit, for the case that a sad frame from a
// previous renderer crash is the only remaining RenderFrameHost in a process.
// See https://crbug.com/1226834.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       AllowUnusedProcessToExitAfterCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure all sites get dedicated processes during the test.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  GURL initial_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* child_rfh0 = root->child_at(0)->current_frame_host();
  RenderFrameHostImpl* child_rfh1 = root->child_at(1)->current_frame_host();
  RenderViewHostImpl* rvh_b = child_rfh0->render_view_host();
  int process_b_id = child_rfh0->GetProcess()->GetID();
  EXPECT_EQ(child_rfh0->GetProcess(), child_rfh1->GetProcess());
  EXPECT_TRUE(child_rfh0->GetProcess()->IsInitializedAndNotDead());
  EXPECT_TRUE(child_rfh0->IsRenderFrameLive());
  EXPECT_TRUE(child_rfh1->IsRenderFrameLive());
  EXPECT_TRUE(rvh_b->IsRenderViewLive());

  // Reset the process exit related counts.
  process_exits_ = 0;
  host_destructions_ = 0;
  Observe(child_rfh0->GetProcess());

  // Terminate the subframe process.
  {
    RenderProcessHostWatcher termination_observer(
        child_rfh0->GetProcess(),
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_rfh0->GetProcess()->Shutdown(0);
    termination_observer.Wait();
  }
  EXPECT_FALSE(child_rfh0->GetProcess()->IsInitializedAndNotDead());
  EXPECT_FALSE(child_rfh0->IsRenderFrameLive());
  EXPECT_FALSE(child_rfh1->IsRenderFrameLive());
  EXPECT_FALSE(rvh_b->IsRenderViewLive());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);

  // Reset the process exit related counts.
  process_exits_ = 0;
  host_destructions_ = 0;

  // Reload the first frame but not the second. This will replace child_rfh0
  // with a new RFH in the same SiteInstance and process as before.
  {
    TestFrameNavigationObserver reload_observer(root->child_at(0));
    std::string reload_script(
        "var f = document.getElementById('child-0');"
        "f.src = f.src;");
    EXPECT_TRUE(ExecJs(root, reload_script));
    reload_observer.Wait();
  }
  RenderFrameHostImpl* new_child_rfh0 = root->child_at(0)->current_frame_host();
  EXPECT_EQ(child_rfh1->GetProcess(), new_child_rfh0->GetProcess());
  EXPECT_TRUE(new_child_rfh0->GetProcess()->IsInitializedAndNotDead());
  EXPECT_TRUE(new_child_rfh0->IsRenderFrameLive());
  EXPECT_FALSE(child_rfh1->IsRenderFrameLive());

  // Navigate the first frame to a different process. This again replaces
  // new_child_rfh0 with a new RFH, now in a different process.
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));
  RenderFrameDeletedObserver rfh_deleted_observer(new_child_rfh0);
  TestFrameNavigationObserver navigation_observer(root->child_at(0));
  EXPECT_TRUE(NavigateToURLFromRenderer(root->child_at(0), url_c));
  navigation_observer.Wait();
  rfh_deleted_observer.WaitUntilDeleted();
  EXPECT_NE(child_rfh1->GetProcess(),
            root->child_at(0)->current_frame_host()->GetProcess());

  // This used to leave the b.com process running, since child_rfh1 was still
  // counted as an active frame in the RenderProcessHost even though it wasn't
  // live.
  EXPECT_FALSE(child_rfh1->GetProcess()->IsInitializedAndNotDead());
  EXPECT_FALSE(child_rfh1->IsRenderFrameLive());
  EXPECT_FALSE(rvh_b->IsRenderViewLive());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(0, host_destructions_);

  // There shouldn't be live proxies either.
  RenderFrameProxyHost* proxy =
      root->current_frame_host()
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(child_rfh1->GetSiteInstance()->group());
  EXPECT_FALSE(proxy->is_render_frame_proxy_live());

  // Reset the process exit related counts.
  process_exits_ = 0;
  host_destructions_ = 0;

  // After the leak cleanup, reload the second frame to make sure it still works
  // in the original RenderProcessHost, even though it swaps to a new
  // RenderFrameHost.
  {
    TestFrameNavigationObserver reload_observer(root->child_at(1));
    std::string reload_script(
        "var f = document.getElementById('child-1');"
        "f.src = f.src;");
    EXPECT_TRUE(ExecJs(root, reload_script));
    reload_observer.Wait();
  }
  RenderFrameHostImpl* new_child_rfh1 = root->child_at(1)->current_frame_host();
  EXPECT_EQ(process_b_id, new_child_rfh1->GetProcess()->GetID());
  EXPECT_TRUE(new_child_rfh1->GetProcess()->IsInitializedAndNotDead());
  EXPECT_TRUE(new_child_rfh1->IsRenderFrameLive());
  EXPECT_EQ(0, process_exits_);
  EXPECT_EQ(0, host_destructions_);
}

// Test that RenderProcessHostImpl::Cleanup can handle nested deletions of
// RenderFrameHost objects, when we might encounter a parent RFH that is tracked
// among the IPC listeners but is no longer discoverable via FromID, while
// handling the deletion of a subframe. One way this can occur is during bfcache
// eviction.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, HandleNestedFrameDeletion) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure all sites get dedicated processes during the test.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Navigate to a page with a same-process subframe.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  RenderProcessHost* process_a = rfh_a->GetProcess();
  int process_a_id = process_a->GetID();
  Observe(process_a);

  // Navigate cross-process and evict process A from the back-forward cache.
  // This should not cause a crash when looking for non-live RenderFrameHosts.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  RenderProcessHostWatcher cleanup_observer(
      process_a, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  shell()->web_contents()->GetController().GetBackForwardCache().Flush();
  cleanup_observer.Wait();
  RenderFrameHostImpl* rfh_b = root->current_frame_host();
  EXPECT_NE(process_a_id, rfh_b->GetProcess()->GetID());
  EXPECT_EQ(1, process_exits_);
  EXPECT_EQ(1, host_destructions_);
}

namespace {

// Observer that listens for RenderFrameDeleted and iterates over the remaining
// RenderFrameHosts in the process at the time. This catches a case where a
// parent RenderFrameHost might not be found via RenderFrameHost::FromID because
// of nested frame deletion, which used to cause a CHECK failure.
class RenderFrameDeletionObserver : public WebContentsObserver {
 public:
  explicit RenderFrameDeletionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RenderFrameDeletionObserver() override = default;

  // WebContentsObserver:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    render_frame_deleted_count_++;

    // Find all other RenderFrameHosts in the process, which should exclude the
    // one being deleted.
    std::vector<RenderFrameHost*> all_rfhs;
    RenderProcessHost* process = render_frame_host->GetProcess();
    process->ForEachRenderFrameHost(
        [&all_rfhs](RenderFrameHost* rfh) { all_rfhs.push_back(rfh); });

    // Update the cumulative count of other RenderFrameHosts in the process.
    render_frame_host_iterator_count_ += all_rfhs.size();
  }

  // Returns how many time RenderFrameDeleted was called.
  int render_frame_deleted_count() { return render_frame_deleted_count_; }

  // Returns a cumulative count of how many remaining RenderFrameHosts were
  // found in the process at the time of any RenderFrameDeleted calls.
  int render_frame_host_iterator_count() {
    return render_frame_host_iterator_count_;
  }

 private:
  int render_frame_deleted_count_ = 0;
  int render_frame_host_iterator_count_ = 0;
};

}  // namespace

// Test that RenderProcessHost::ForEachRenderFrameHost can handle nested
// deletions of RenderFrameHost objects, when we might encounter a parent RFH
// that is no longer discoverable via FromID, while handling the deletion of a
// subframe. One way this can occur is during bfcache eviction.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, ForEachFrameNestedFrameDeletion) {
  // This test specifically wants to test with BackForwardCache eviction, so
  // skip it if BackForwardCache is disabled.
  if (!IsBackForwardCacheEnabled())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure all sites get dedicated processes during the test.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Navigate to a page with a same-process subframe.
  GURL url_a(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  RenderProcessHost* process_a = rfh_a->GetProcess();
  int process_a_id = process_a->GetID();

  // Listen for RenderFrameDeleted and count the other RenderFrameHosts in the
  // process at the time.
  RenderFrameDeletionObserver rfh_deletion_observer(shell()->web_contents());

  // Navigate cross-process and evict process A from the back-forward cache.
  // This should not cause a crash when iterating over RenderFrameHosts.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  RenderProcessHostWatcher cleanup_observer(
      process_a, RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  shell()->web_contents()->GetController().GetBackForwardCache().Flush();
  cleanup_observer.Wait();
  RenderFrameHostImpl* rfh_b = root->current_frame_host();
  EXPECT_NE(process_a_id, rfh_b->GetProcess()->GetID());

  // RenderFrameDeleted should have been called for both the main frame and
  // subframe in process A.
  EXPECT_EQ(2, rfh_deletion_observer.render_frame_deleted_count());

  // The subframe's RenderFrameDeleted happens after both the main frame and
  // subframe have become undiscoverable by FromID (i.e., removed from
  // g_routing_id_frame_map). The subframe isn't expected to be found during its
  // own RenderFrameDeleted (since it has been removed from
  // render_frame_host_id_set_), but the partially destructed main frame also
  // isn't found at that time when iterating over other frames in the process.
  EXPECT_EQ(0, rfh_deletion_observer.render_frame_host_iterator_count());
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, ZeroExecutionTimes) {
  // This test only works if the renderer process is sandboxed.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kNoSandbox)) {
    return;
  }
  base::HistogramTester histogram_tester;
  RenderProcessHost* process = RenderProcessHostImpl::CreateRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context(), nullptr);
  RenderProcessHostWatcher process_watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY);
  process->Init();
  process_watcher.Wait();
  EXPECT_TRUE(process->IsReady());
  histogram_tester.ExpectUniqueSample(
      "BrowserRenderProcessHost.SuspendedChild.UserExecutionRecorded", false,
      1);
  histogram_tester.ExpectUniqueSample(
      "BrowserRenderProcessHost.SuspendedChild.KernelExecutionRecorded", false,
      1);
  process->Cleanup();
}

class RenderProcessHostWriteableFileTest
    : public RenderProcessHostTestBase,
      public ::testing::WithParamInterface<
          std::tuple</*enforcement_enabled=*/bool,
                     /*add_no_execute_flags=*/bool>> {
 public:
  void SetUp() override {
    enforcement_feature_.InitWithFeatureState(
        base::features::kEnforceNoExecutableFileHandles,
        IsEnforcementEnabled());
    RenderProcessHostTestBase::SetUp();
  }

 protected:
  bool IsEnforcementEnabled() { return std::get<0>(GetParam()); }
  bool ShouldMarkNoExecute() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList enforcement_feature_;
};

// This test verifies that the renderer process is wired up correctly with the
// mojo invitation flag that indicates that it's untrusted. The other half of
// this test that verifies that a security violation actually causes a DCHECK
// lives in mojo/core, and can't live here as death tests are not supported for
// browser tests.
IN_PROC_BROWSER_TEST_P(RenderProcessHostWriteableFileTest,
                       PassUnsafeWriteableExecutableFile) {
  // This test only works if DCHECKs are enabled.
#if !DCHECK_IS_ON()
  GTEST_SKIP();
#else
  // This test only works if the renderer process is sandboxed.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kNoSandbox)) {
    GTEST_SKIP();
  }

  base::ScopedAllowBlockingForTesting allow_blocking;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  mojo::Remote<mojom::TestService> test_service;
  rph->BindReceiver(test_service.BindNewPipeAndPassReceiver());

  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                   base::File::FLAG_WRITE;
  if (ShouldMarkNoExecute()) {
    flags = base::File::AddFlagsForPassingToUntrustedProcess(flags);
  }

  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  base::File temp_file_writeable(file_path, flags);
  ASSERT_TRUE(temp_file_writeable.IsValid());

  bool error_was_called = false;
  mojo::SetUnsafeFileHandleCallbackForTesting(
      base::BindLambdaForTesting([&error_was_called]() -> bool {
        error_was_called = true;
        return true;
      }));

  base::RunLoop run_loop;
  test_service->PassWriteableFile(std::move(temp_file_writeable),
                                  run_loop.QuitClosure());
  run_loop.Run();

  // This test should only detect a violation if enforcement is enabled and the
  // file has not been marked no-execute correctly.
  bool should_violation_occur =
      IsEnforcementEnabled() && !ShouldMarkNoExecute();
  EXPECT_EQ(should_violation_occur, error_was_called);
#endif  // DCHECK_IS_ON()
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RenderProcessHostWriteableFileTest,
    testing::Combine(/*enforcement_enabled=*/testing::Bool(),
                     /*add_no_execute_flags=*/testing::Bool()));

#endif  // BUILDFLAG(IS_WIN)

// This test verifies that the Pseudonymization salt that is generated in the
// browser process is correctly synchronized with a child process, in this case,
// two separate renderer processes.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       SetPseudonymizationSaltSynchronized) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure all sites get dedicated processes during the test.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  // Create two renderer processes.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/simple_page.html")));
  RenderProcessHost* rph1 =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, embedded_test_server()->GetURL(
                                              "b.com", "/simple_page.html")));
  RenderProcessHost* rph2 =
      second_shell->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // This test needs two processes.
  EXPECT_NE(rph1->GetProcess().Pid(), rph2->GetProcess().Pid());

  const std::string test_string = "testing123";
  uint32_t browser_result =
      PseudonymizationUtil::PseudonymizeStringForTesting(test_string);

  for (RenderProcessHost* rph : {rph1, rph2}) {
    mojo::Remote<mojom::TestService> service;
    rph->BindReceiver(service.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;

    std::optional<uint32_t> renderer_result = std::nullopt;
    service->PseudonymizeString(
        test_string, base::BindLambdaForTesting([&](uint32_t result) {
          renderer_result = result;
          run_loop.Quit();
        }));
    run_loop.Run();

    ASSERT_TRUE(renderer_result.has_value());
    EXPECT_EQ(*renderer_result, browser_result);
  }
}

class CreationObserver : public RenderProcessHostCreationObserver {
 public:
  explicit CreationObserver(
      base::RepeatingClosure closure = base::RepeatingClosure())
      : closure_(std::move(closure)) {}

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(RenderProcessHost* process_host) override {
    if (closure_) {
      closure_.Run();
    }
  }

 private:
  base::RepeatingClosure closure_;
};

IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, HostCreationObserved) {
  int created_count = 0;
  CreationObserver creation_observer(
      base::BindLambdaForTesting([&created_count]() { ++created_count; }));
  RenderProcessHost* process = RenderProcessHostImpl::CreateRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context(), nullptr);
  RenderProcessHostWatcher process_watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY);
  process->Init();
  process_watcher.Wait();
  ASSERT_TRUE(process->IsReady());
  EXPECT_EQ(1, created_count);
  process->Cleanup();
}

// Notification of RenderProcessHost creation should not crash if creation
// observers are added during notification of another creation observer.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       HostCreationObserversAddedDuringNotification) {
  std::vector<std::unique_ptr<CreationObserver>> added_creation_observers;
  const int kObserversToAdd = 1000;
  int added_observer_notification_count = 0;
  const auto increment_added_observer_notification_count =
      base::BindLambdaForTesting([&added_observer_notification_count]() {
        ++added_observer_notification_count;
      });
  CreationObserver creation_observer1(base::BindLambdaForTesting(
      [&added_creation_observers,
       increment_added_observer_notification_count]() {
        for (int i = 0; i < kObserversToAdd; ++i) {
          added_creation_observers.push_back(std::make_unique<CreationObserver>(
              increment_added_observer_notification_count));
        }
      }));
  CreationObserver creation_observer2;

  RenderProcessHost* process = RenderProcessHostImpl::CreateRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context(), nullptr);
  RenderProcessHostWatcher process_watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY);
  process->Init();
  process_watcher.Wait();
  EXPECT_TRUE(process->IsReady());
  EXPECT_EQ(kObserversToAdd, added_observer_notification_count);
  process->Cleanup();
}

// Notification of RenderProcessHost creation should not crash if a creation
// observer is destroyed during notification of another creation observer.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest,
                       HostCreationObserversDestroyedDuringNotification) {
  base::OnceClosure destroy_second_observer;
  CreationObserver creation_observer1(
      base::BindLambdaForTesting([&destroy_second_observer]() {
        std::move(destroy_second_observer).Run();
      }));
  auto creation_observer2 = std::make_unique<CreationObserver>();
  destroy_second_observer = base::BindLambdaForTesting(
      [&creation_observer2]() { creation_observer2.reset(); });

  RenderProcessHost* process = RenderProcessHostImpl::CreateRenderProcessHost(
      ShellContentBrowserClient::Get()->browser_context(), nullptr);
  RenderProcessHostWatcher process_watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY);
  process->Init();
  process_watcher.Wait();
  EXPECT_TRUE(process->IsReady());
  EXPECT_EQ(nullptr, creation_observer2.get());
  process->Cleanup();
}

namespace {

bool FetchScript(Shell* shell, GURL url) {
  EvalJsResult result = EvalJs(shell, JsReplace(R"(
      new Promise(resolve => {
        const script = document.createElement("script");
        script.src = $1;
        script.onerror = () => resolve("error");
        script.onload = () => resolve("fetched");
        document.body.appendChild(script);
      });
    )",
                                                url));
  return result.ExtractString() == "fetched";
}

}  // namespace

// Tests that BrowsingDataRemover clears renderer's in-memory resource cache.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, ClearResourceCache) {
  constexpr const char* kScriptPath = "/cacheable.js";

  // Count the number of requests from the renderer. This doesn't count requests
  // that are served via the renderer's in-memory cache.
  size_t num_script_requests_from_renderer = 0;
  embedded_test_server()->RegisterRequestMonitor(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request) {
        if (request.relative_url == kScriptPath) {
          ++num_script_requests_from_renderer;
        }
      }));

  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kScriptUrl = embedded_test_server()->GetURL(kScriptPath);

  EXPECT_TRUE(NavigateToURL(shell(), kUrl));

  // The first fetch. The renderer's in-memory cache doesn't contain a response
  // so the counter should be incremented.
  EXPECT_TRUE(FetchScript(shell(), kScriptUrl));
  ASSERT_EQ(num_script_requests_from_renderer, 1u);

  // The second fetch. The response will be served from the renderer's in-memory
  // cache. The counter should not be incremented.
  EXPECT_TRUE(FetchScript(shell(), kScriptUrl));
  ASSERT_EQ(num_script_requests_from_renderer, 1u);

  // Clear the renderer's in-memory cache.
  BrowsingDataRemover* remover =
      shell()->web_contents()->GetBrowserContext()->GetBrowsingDataRemover();
  BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      /*delete_begin=*/base::Time(), /*delete_end=*/base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_CACHE,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();

  // Fetch again. The response in the renderer's in-memory cache was evicted so
  // the counter should be incremented.
  EXPECT_TRUE(FetchScript(shell(), kScriptUrl));
  ASSERT_EQ(num_script_requests_from_renderer, 2u);
}

// Tests that RenderProcessHost reuse works correctly even if the site URL of a
// URL changes.
IN_PROC_BROWSER_TEST_P(RenderProcessHostTest, ReuseSiteURLChanges) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl = embedded_test_server()->GetURL("/title1.html");
  const GURL kModifiedSiteUrl("custom-scheme://custom");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(context, kUrl);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            site_instance->GetProcess());

  // Have the main frame navigate to the first url. Getting a RenderProcessHost
  // with the REUSE_PENDING_OR_COMMITTED_SITE policy should now return the
  // process of the main RFH.
  EXPECT_TRUE(NavigateToURL(shell(), kUrl));
  site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(context, kUrl);
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            site_instance->GetProcess());

  // Install the custom ContentBrowserClient. Site URLs are now modified.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as the RFH is
  // registered with the normal site URL.
  {
    EffectiveURLContentBrowserTestContentBrowserClient modified_client(
        kUrl, kModifiedSiteUrl,
        /* requires_dedicated_process */ false);
    site_instance =
        SiteInstanceImpl::CreateReusableInstanceForTesting(context, kUrl);
    EXPECT_NE(root->current_frame_host()->GetProcess(),
              site_instance->GetProcess());

    // Reload. Getting a RenderProcessHost with the
    // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of
    // the main RFH, as it is now registered with the modified site URL.
    shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    site_instance =
        SiteInstanceImpl::CreateReusableInstanceForTesting(context, kUrl);
    EXPECT_EQ(root->current_frame_host()->GetProcess(),
              site_instance->GetProcess());
  }

  // Remove the custom ContentBrowserClient. Site URLs are back to normal.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as it is registered
  // with the modified site URL.
  site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(context, kUrl);
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            site_instance->GetProcess());

  // Reload. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH, as it is now registered with the regular site URL.
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(context, kUrl);
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            site_instance->GetProcess());
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
class FakeStableVideoDecoderFactoryService
    : public media::stable::mojom::StableVideoDecoderFactory {
 public:
  FakeStableVideoDecoderFactoryService() = default;
  FakeStableVideoDecoderFactoryService(
      const FakeStableVideoDecoderFactoryService&) = delete;
  FakeStableVideoDecoderFactoryService& operator=(
      const FakeStableVideoDecoderFactoryService&) = delete;
  ~FakeStableVideoDecoderFactoryService() override = default;

  // media::stable::mojom::StableVideoDecoderFactory implementation.
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoderTracker>
          tracker) final {
    video_decoders_.Add(
        std::make_unique<FakeStableVideoDecoderService>(std::move(tracker)),
        std::move(receiver));
  }

 private:
  class FakeStableVideoDecoderService
      : public media::stable::mojom::StableVideoDecoder {
   public:
    explicit FakeStableVideoDecoderService(
        mojo::PendingRemote<media::stable::mojom::StableVideoDecoderTracker>
            tracker)
        : tracker_(std::move(tracker)) {}
    FakeStableVideoDecoderService(const FakeStableVideoDecoderService&) =
        delete;
    FakeStableVideoDecoderService& operator=(
        const FakeStableVideoDecoderService&) = delete;
    ~FakeStableVideoDecoderService() override = default;

    // media::stable::mojom::StableVideoDecoder implementation.
    void GetSupportedConfigs(GetSupportedConfigsCallback callback) final {
      std::move(callback).Run({}, media::VideoDecoderType::kTesting);
    }
    void Construct(
        mojo::PendingAssociatedRemote<media::stable::mojom::VideoDecoderClient>
            stable_video_decoder_client_remote,
        mojo::PendingRemote<media::stable::mojom::MediaLog>
            stable_media_log_remote,
        mojo::PendingReceiver<media::stable::mojom::VideoFrameHandleReleaser>
            stable_video_frame_handle_releaser_receiver,
        mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
        const gfx::ColorSpace& target_color_space) final {}
    void Initialize(
        const media::VideoDecoderConfig& config,
        bool low_delay,
        mojo::PendingRemote<media::stable::mojom::StableCdmContext> cdm_context,
        InitializeCallback callback) final {}
    void Decode(const scoped_refptr<media::DecoderBuffer>& buffer,
                DecodeCallback callback) final {}
    void Reset(ResetCallback callback) final {}

   private:
    mojo::Remote<media::stable::mojom::StableVideoDecoderTracker> tracker_;
  };

  mojo::UniqueReceiverSet<media::stable::mojom::StableVideoDecoder>
      video_decoders_;
};

class RenderProcessHostTestStableVideoDecoderTest
    : public RenderProcessHostTestBase {
 public:
  RenderProcessHostTestStableVideoDecoderTest()
      : stable_video_decoder_factory_receiver_(
            &stable_video_decoder_factory_service_) {}

  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kUseOutOfProcessVideoDecoding);
    RenderProcessHostTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    RenderProcessHostImpl::SetStableVideoDecoderFactoryCreationCBForTesting(
        stable_video_decoder_factory_creation_cb_.Get());
    RenderProcessHostImpl::SetStableVideoDecoderEventCBForTesting(
        stable_video_decoder_event_cb_.Get());

#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
    // When Chrome is compiled with
    // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT), renderer processes need a
    // media::mojom::VideoDecoder during startup in order to query for supported
    // configurations (see content::RenderMediaClient::Initialize()). With
    // OOP-VD, this should cause the creation of a
    // media::stable::mojom::StableVideoDecoderFactory in order to create the
    // corresponding media::stable::mojom::StableVideoDecoder. When the
    // supported configurations are obtained, the media::mojom::VideoDecoder and
    // media::stable::mojom::StableVideoDecoder connections should be torn down
    // thus causing the termination of the
    // media::stable::mojom::StableVideoDecoderFactory connection. Here, we set
    // up expectations for that.
    base::RunLoop run_loop;
    {
      InSequence seq;
      EXPECT_CALL(stable_video_decoder_factory_creation_cb_, Run(_))
          .WillOnce(
              [&](mojo::PendingReceiver<
                  media::stable::mojom::StableVideoDecoderFactory> receiver) {
                stable_video_decoder_factory_receiver_.Bind(
                    std::move(receiver));
                stable_video_decoder_factory_receiver_.set_disconnect_handler(
                    stable_video_decoder_factory_disconnect_cb_.Get());
              });
      EXPECT_CALL(stable_video_decoder_event_cb_,
                  Run(RenderProcessHostImpl::StableVideoDecoderEvent::
                          kAllDecodersDisconnected));
      EXPECT_CALL(stable_video_decoder_factory_disconnect_cb_, Run())
          .WillOnce([&]() {
            stable_video_decoder_factory_receiver_.reset();
            run_loop.Quit();
          });
    }
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)

    rph_ = RenderProcessHostImpl::CreateRenderProcessHost(
        ShellContentBrowserClient::Get()->browser_context(), nullptr);
    ASSERT_TRUE(rph_->Init());
    rph_initialized_ = true;

#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
    run_loop.Run();
    ASSERT_TRUE(VerifyAndClearExpectations());
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
  }

  void TearDownOnMainThread() override {
    // Reset the |stable_video_decoder_factory_receiver_| so that the
    // disconnection callback is not called on tear down.
    stable_video_decoder_factory_receiver_.reset();
    if (rph_initialized_) {
      rph_->Cleanup();
    }
    rph_ = nullptr;
  }

 protected:
  bool VerifyAndClearExpectations() {
    // Note: we verify and clear the expectations for all the mocks. We
    // intentionally don't early out if verifying one mock fails.
    bool result = Mock::VerifyAndClearExpectations(
        &stable_video_decoder_factory_creation_cb_);
    result = Mock::VerifyAndClearExpectations(
                 &stable_video_decoder_factory_disconnect_cb_) &&
             result;
    result =
        Mock::VerifyAndClearExpectations(&stable_video_decoder_event_cb_) &&
        result;
    return result;
  }

  base::test::ScopedFeatureList feature_list_;

  StrictMock<base::MockRepeatingCallback<
      RenderProcessHostImpl::StableVideoDecoderFactoryCreationCB::RunType>>
      stable_video_decoder_factory_creation_cb_;
  StrictMock<base::MockOnceCallback<void()>>
      stable_video_decoder_factory_disconnect_cb_;
  StrictMock<base::MockRepeatingCallback<
      RenderProcessHostImpl::StableVideoDecoderEventCB::RunType>>
      stable_video_decoder_event_cb_;

  FakeStableVideoDecoderFactoryService stable_video_decoder_factory_service_;
  mojo::Receiver<media::stable::mojom::StableVideoDecoderFactory>
      stable_video_decoder_factory_receiver_;

  raw_ptr<RenderProcessHost> rph_ = nullptr;
  bool rph_initialized_ = false;
};

// Ensures that the StableVideoDecoderFactory connection is terminated after a
// delay once all the StableVideoDecoders created with it have disconnected.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTestStableVideoDecoderTest,
                       FactoryIsResetAfterDelay) {
  ASSERT_FALSE(Test::HasFailure());

  // First, let's ask the RPH to establish a StableVideoDecoder connection. This
  // should cause the RPH's StableVideoDecoderFactory to be bound.
  EXPECT_CALL(stable_video_decoder_factory_creation_cb_, Run(_))
      .WillOnce([&](mojo::PendingReceiver<
                    media::stable::mojom::StableVideoDecoderFactory> receiver) {
        stable_video_decoder_factory_receiver_.Bind(std::move(receiver));
        stable_video_decoder_factory_receiver_.set_disconnect_handler(
            stable_video_decoder_factory_disconnect_cb_.Get());
      });
  mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
      stable_video_decoder_remote;
  rph_->CreateStableVideoDecoder(
      stable_video_decoder_remote.InitWithNewPipeAndPassReceiver());
  ASSERT_TRUE(VerifyAndClearExpectations());

  // Now, let's destroy the StableVideoDecoder connection. Since this was the
  // only StableVideoDecoder connection, destroying it should cause the RPH's
  // StableVideoDecoderFactory connection to die after a delay.
  base::RunLoop run_loop;
  base::ElapsedTimer reset_stable_video_decoder_factory_timer;
  {
    InSequence seq;
    EXPECT_CALL(stable_video_decoder_event_cb_,
                Run(RenderProcessHostImpl::StableVideoDecoderEvent::
                        kAllDecodersDisconnected));
    EXPECT_CALL(stable_video_decoder_factory_disconnect_cb_, Run())
        .WillOnce([&]() { run_loop.Quit(); });
  }
  stable_video_decoder_remote.reset();
  run_loop.Run();
  EXPECT_GE(reset_stable_video_decoder_factory_timer.Elapsed(),
            base::Seconds(3));
}

// Ensures that the timer that destroys the StableVideoDecoderFactory connection
// when all StableVideoDecoder connections die is stopped if a request to
// connect another StableVideoDecoder is received soon enough.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTestStableVideoDecoderTest,
                       FactoryResetTimerIsStoppedOnRequestBeforeResetDelay) {
  ASSERT_FALSE(Test::HasFailure());

  // First, let's ask the RPH to establish a StableVideoDecoder connection. This
  // should cause the RPH's StableVideoDecoderFactory to be bound.
  EXPECT_CALL(stable_video_decoder_factory_creation_cb_, Run(_))
      .WillOnce([&](mojo::PendingReceiver<
                    media::stable::mojom::StableVideoDecoderFactory> receiver) {
        stable_video_decoder_factory_receiver_.Bind(std::move(receiver));
        stable_video_decoder_factory_receiver_.set_disconnect_handler(
            stable_video_decoder_factory_disconnect_cb_.Get());
      });
  mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
      stable_video_decoder_remote;
  rph_->CreateStableVideoDecoder(
      stable_video_decoder_remote.InitWithNewPipeAndPassReceiver());
  ASSERT_TRUE(VerifyAndClearExpectations());

  // Now, let's destroy the StableVideoDecoder connection. Since this was the
  // only StableVideoDecoder connection, destroying it should trigger a
  // kAllDecodersDisconnected event.
  base::RunLoop run_loop_1;
  EXPECT_CALL(stable_video_decoder_event_cb_,
              Run(RenderProcessHostImpl::StableVideoDecoderEvent::
                      kAllDecodersDisconnected))
      .WillOnce([&]() { run_loop_1.Quit(); });
  stable_video_decoder_remote.reset();
  run_loop_1.Run();
  ASSERT_TRUE(VerifyAndClearExpectations());

  // Now, let's request another StableVideoDecoder connection immediately. This
  // should stop the timer that resets the factory.
  EXPECT_CALL(stable_video_decoder_event_cb_,
              Run(RenderProcessHostImpl::StableVideoDecoderEvent::
                      kFactoryResetTimerStopped));
  rph_->CreateStableVideoDecoder(
      stable_video_decoder_remote.InitWithNewPipeAndPassReceiver());
  ASSERT_TRUE(VerifyAndClearExpectations());

  // Finally, let's wait a few seconds (longer than the delay configured for the
  // timer that kills the StableVideoDecoderFactory connection). Because the
  // |stable_video_decoder_factory_disconnect_cb_| is a StrictMock, this should
  // detect that the StableVideoDecoderFactory connection doesn't die.
  base::RunLoop run_loop_2;
  GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
          run_loop_2.QuitClosure()),
      base::Seconds(5));
  run_loop_2.Run();
  ASSERT_TRUE(VerifyAndClearExpectations());
}

#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

}  // namespace content
