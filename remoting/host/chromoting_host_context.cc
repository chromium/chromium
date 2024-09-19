// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ChromotingHostContextChromeOs : public ChromotingHostContext {
 public:
  ChromotingHostContextChromeOs(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
      scoped_refptr<AutoThreadTaskRunner> audio_task_runner,
      scoped_refptr<AutoThreadTaskRunner> file_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> network_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  ChromotingHostContextChromeOs(const ChromotingHostContextChromeOs&) = delete;
  ChromotingHostContextChromeOs& operator=(
      const ChromotingHostContextChromeOs&) = delete;

  ~ChromotingHostContextChromeOs() override;

  // remoting::ChromotingHostContext implementation.
  std::unique_ptr<ChromotingHostContext> Copy() override;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter()
      const override;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() override;

 private:
  // |ui_shared_url_loader_factory_| is a SharedUrlLoaderFactory which is bound
  // to the ui_task_runner sequence and is used to create copies of the original
  // ChromotingHostContext instance.
  scoped_refptr<network::SharedURLLoaderFactory> ui_shared_url_loader_factory_;

  // |pending_factory_| is initialized from |ui_shared_url_loader_factory_| on
  // the UI thread which allows for binding |network_shared_url_loader_factory_|
  // to the network_task_runner sequence.
  std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory_;

  // |network_shared_url_loader_factory_| is a SharedUrlLoaderFactory which is
  // bound to the network_task_runner sequence.
  scoped_refptr<network::SharedURLLoaderFactory>
      network_shared_url_loader_factory_;
};

ChromotingHostContextChromeOs::ChromotingHostContextChromeOs(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<AutoThreadTaskRunner> audio_task_runner,
    scoped_refptr<AutoThreadTaskRunner> file_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> network_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : ChromotingHostContext(ui_task_runner,
                            audio_task_runner,
                            file_task_runner,
                            input_task_runner,
                            network_task_runner,
                            video_capture_task_runner,
                            video_encode_task_runner),
      ui_shared_url_loader_factory_(shared_url_loader_factory),
      pending_factory_(ui_shared_url_loader_factory_->Clone()) {}

ChromotingHostContextChromeOs::~ChromotingHostContextChromeOs() {
  // |ui_shared_url_loader_factory_| should always be valid however
  // |network_shared_url_loader_factory_| may not be if it was never accessed.
  ui_task_runner()->ReleaseSoon(FROM_HERE,
                                std::move(ui_shared_url_loader_factory_));
  if (network_shared_url_loader_factory_) {
    network_task_runner()->ReleaseSoon(
        FROM_HERE, std::move(network_shared_url_loader_factory_));
  }
}

std::unique_ptr<ChromotingHostContext> ChromotingHostContextChromeOs::Copy() {
  DCHECK(ui_task_runner()->BelongsToCurrentThread());
  return std::make_unique<ChromotingHostContextChromeOs>(
      ui_task_runner(), audio_task_runner(), file_task_runner(),
      input_task_runner(), network_task_runner(), video_capture_task_runner(),
      video_encode_task_runner(), ui_shared_url_loader_factory_);
}

scoped_refptr<net::URLRequestContextGetter>
ChromotingHostContextChromeOs::url_request_context_getter() const {
  NOTREACHED();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromotingHostContextChromeOs::url_loader_factory() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  if (!network_shared_url_loader_factory_) {
    network_shared_url_loader_factory_ =
        network::SharedURLLoaderFactory::Create(std::move(pending_factory_));
  }
  return network_shared_url_loader_factory_;
}
#else  // !BUILDFLAG(IS_CHROMEOS_ASH)
void DisallowBlockingOperations() {
  base::DisallowBlocking();
  // TODO(crbug.com/41360128): Re-enable after the underlying issue is fixed.
  // base::DisallowBaseSyncPrimitives();
}

class ChromotingHostContextDesktop : public ChromotingHostContext {
 public:
  ChromotingHostContextDesktop(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
      scoped_refptr<AutoThreadTaskRunner> audio_task_runner,
      scoped_refptr<AutoThreadTaskRunner> file_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> network_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  ChromotingHostContextDesktop(const ChromotingHostContextDesktop&) = delete;
  ChromotingHostContextDesktop& operator=(const ChromotingHostContextDesktop&) =
      delete;

  ~ChromotingHostContextDesktop() override;

  // remoting::ChromotingHostContext implementation.
  std::unique_ptr<ChromotingHostContext> Copy() override;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter()
      const override;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() override;

 private:
  // Serves URLRequestContexts that use the network and UI task runners.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // Makes a SharedURLLoaderFactory out of |url_request_context_getter_|
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
};

ChromotingHostContextDesktop::ChromotingHostContextDesktop(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<AutoThreadTaskRunner> audio_task_runner,
    scoped_refptr<AutoThreadTaskRunner> file_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> network_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : ChromotingHostContext(ui_task_runner,
                            audio_task_runner,
                            file_task_runner,
                            input_task_runner,
                            network_task_runner,
                            video_capture_task_runner,
                            video_encode_task_runner),
      url_request_context_getter_(url_request_context_getter) {}

ChromotingHostContextDesktop::~ChromotingHostContextDesktop() {
  if (url_loader_factory_owner_) {
    network_task_runner()->DeleteSoon(FROM_HERE,
                                      url_loader_factory_owner_.release());
  }
}

std::unique_ptr<ChromotingHostContext> ChromotingHostContextDesktop::Copy() {
  return std::make_unique<ChromotingHostContextDesktop>(
      ui_task_runner(), audio_task_runner(), file_task_runner(),
      input_task_runner(), network_task_runner(), video_capture_task_runner(),
      video_encode_task_runner(), url_request_context_getter_);
}

scoped_refptr<net::URLRequestContextGetter>
ChromotingHostContextDesktop::url_request_context_getter() const {
  return url_request_context_getter_;
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromotingHostContextDesktop::url_loader_factory() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  if (!url_loader_factory_owner_) {
    url_loader_factory_owner_ =
        std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
            url_request_context_getter_, /* is_trusted= */ true);
  }
  return url_loader_factory_owner_->GetURLLoaderFactory();
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

ChromotingHostContext::ChromotingHostContext(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<AutoThreadTaskRunner> audio_task_runner,
    scoped_refptr<AutoThreadTaskRunner> file_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> network_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner)
    : ui_task_runner_(ui_task_runner),
      audio_task_runner_(audio_task_runner),
      file_task_runner_(file_task_runner),
      input_task_runner_(input_task_runner),
      network_task_runner_(network_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      video_encode_task_runner_(video_encode_task_runner) {}

ChromotingHostContext::~ChromotingHostContext() = default;

scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::audio_task_runner()
    const {
  return audio_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::file_task_runner()
    const {
  return file_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::input_task_runner()
    const {
  return input_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::network_task_runner()
    const {
  return network_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::ui_task_runner()
    const {
  return ui_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::video_capture_task_runner() const {
  return video_capture_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::video_encode_task_runner() const {
  return video_encode_task_runner_;
}

policy::ManagementService* ChromotingHostContext::management_service() {
  return policy::PlatformManagementService::GetInstance();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<ChromotingHostContext> ChromotingHostContext::Create(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner) {
#if BUILDFLAG(IS_WIN)
  // On Windows the AudioCapturer requires COM, so we run a single-threaded
  // apartment, which requires a UI thread.
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner =
      AutoThread::CreateWithLoopAndComInitTypes(
          "ChromotingAudioThread", ui_task_runner, base::MessagePumpType::UI,
          AutoThread::COM_INIT_STA);
#else   // !BUILDFLAG(IS_WIN)
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner =
      AutoThread::CreateWithType("ChromotingAudioThread", ui_task_runner,
                                 base::MessagePumpType::IO);
#endif  // !BUILDFLAG(IS_WIN)
  scoped_refptr<AutoThreadTaskRunner> file_task_runner =
      AutoThread::CreateWithType("ChromotingFileThread", ui_task_runner,
                                 base::MessagePumpType::IO);

  scoped_refptr<AutoThreadTaskRunner> network_task_runner =
      AutoThread::CreateWithType("ChromotingNetworkThread", ui_task_runner,
                                 base::MessagePumpType::IO);
  network_task_runner->PostTask(FROM_HERE,
                                base::BindOnce(&DisallowBlockingOperations));

  // InputInjectorX11 requires an X11EventSource, which can only be created
  // on a UI thread.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner =
      AutoThread::CreateWithType("ChromotingInputThread", ui_task_runner,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
                                 base::MessagePumpType::UI);
#else
                                 base::MessagePumpType::IO);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  return std::make_unique<ChromotingHostContextDesktop>(
      ui_task_runner, audio_task_runner, file_task_runner, input_task_runner,
      network_task_runner,
#if BUILDFLAG(IS_APPLE)
      // Mac requires a UI thread for the capturer.
      AutoThread::CreateWithType("ChromotingCaptureThread", ui_task_runner,
                                 base::MessagePumpType::UI),
#else   // !BUILDFLAG(IS_APPLE)
      AutoThread::Create("ChromotingCaptureThread", ui_task_runner),
#endif  // !BUILDFLAG(IS_APPLE)
      AutoThread::Create("ChromotingEncodeThread", ui_task_runner),
      base::MakeRefCounted<URLRequestContextGetter>(network_task_runner));
}
#else   // BUILDFLAG(IS_CHROMEOS_ASH)

// static
std::unique_ptr<ChromotingHostContext> ChromotingHostContext::CreateForChromeOS(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  // AutoThreadTaskRunner is a TaskRunner with the special property that it will
  // continue to process tasks until no references remain. We usually provide a
  // QuitClosure which is run when the AutoThreadTaskRunner instance is
  // destroyed, however on ChromeOS we are running on threads provided by the
  // browser (meaning we don't own them or their lifetime) so we should not be
  // stopping them when a remote session terminates.
  // Providing any sort of callback (even base::DoNothing) will cause a crash if
  // ash-chrome is shutting down when the AutoThreadTaskRunner is being
  // destroyed. A real-world example is starting a CRD session and then signing
  // out, see b/260395047 for more details.
  scoped_refptr<AutoThreadTaskRunner> io_auto_task_runner =
      new AutoThreadTaskRunner(io_task_runner);
  scoped_refptr<AutoThreadTaskRunner> file_auto_task_runner =
      new AutoThreadTaskRunner(file_task_runner);
  scoped_refptr<AutoThreadTaskRunner> ui_auto_task_runner =
      new AutoThreadTaskRunner(ui_task_runner);

  // Use browser's file thread as the joiner as it is the only browser-thread
  // that allows blocking I/O, which is required by thread joining.
  return std::make_unique<ChromotingHostContextChromeOs>(
      ui_auto_task_runner,
      AutoThread::Create("ChromotingAudioThread", file_auto_task_runner),
      file_auto_task_runner,
      ui_auto_task_runner,  // input_task_runner
      io_auto_task_runner,  // network_task_runner
      ui_auto_task_runner,  // video_capture_task_runner
      AutoThread::Create("ChromotingEncodeThread", file_auto_task_runner),
      shared_url_loader_factory);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
std::unique_ptr<ChromotingHostContext> ChromotingHostContext::CreateForTesting(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ChromotingHostContext::CreateForChromeOS(
      ui_task_runner, ui_task_runner, ui_task_runner, url_loader_factory);
#else
  return ChromotingHostContext::Create(ui_task_runner);
#endif
}

}  // namespace remoting
