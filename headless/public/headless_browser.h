// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_H_

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_export.h"
#include "net/base/host_port_pair.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace base {
class MessagePump;
class SingleThreadTaskRunner;
}

namespace headless {

class HeadlessDevToolsChannel;
class HeadlessWebContents;

// This class represents the global headless browser instance. To get a pointer
// to one, call |HeadlessBrowserMain| to initiate the browser main loop. An
// instance of |HeadlessBrowser| will be passed to the callback given to that
// function.
class HEADLESS_EXPORT HeadlessBrowser {
 public:
  struct Options;

  HeadlessBrowser(const HeadlessBrowser&) = delete;
  HeadlessBrowser& operator=(const HeadlessBrowser&) = delete;

  // Create a new browser context which can be used to create tabs and isolate
  // them from one another.
  // Pointer to HeadlessBrowserContext becomes invalid after:
  // a) Calling HeadlessBrowserContext::Close or
  // b) Calling HeadlessBrowser::Shutdown
  virtual HeadlessBrowserContext::Builder CreateBrowserContextBuilder() = 0;

  virtual std::vector<HeadlessBrowserContext*> GetAllBrowserContexts() = 0;

  // Return a DevTools target corresponding to this browser. Note that this
  // method only returns a valid target after browser has been initialized on
  // the main thread. The target only supports the domains available on the
  // browser endpoint excluding the Tethering domain.
  // TODO(dgozman): remove together with HeadlessDevToolsTarget.
  virtual HeadlessDevToolsTarget* GetDevToolsTarget() = 0;

  // Creates a channel connected to the browser. Note that this
  // method only returns a valid channel after browser has been initialized on
  // the main thread. The channel only supports the domains available on the
  // browser endpoint excluding the Tethering domain.
  virtual std::unique_ptr<HeadlessDevToolsChannel> CreateDevToolsChannel() = 0;

  // Returns the HeadlessWebContents associated with the
  // |devtools_agent_host_id| if any.  Otherwise returns null.
  virtual HeadlessWebContents* GetWebContentsForDevToolsAgentHostId(
      const std::string& devtools_agent_host_id) = 0;

  // Returns HeadlessBrowserContext associated with the given id if any.
  // Otherwise returns null.
  virtual HeadlessBrowserContext* GetBrowserContextForId(
      const std::string& id) = 0;

  // Allows setting and getting the browser context that DevTools will create
  // new targets in by default.
  virtual void SetDefaultBrowserContext(
      HeadlessBrowserContext* browser_context) = 0;
  virtual HeadlessBrowserContext* GetDefaultBrowserContext() = 0;

  // Returns a task runner for submitting work to the browser main thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const = 0;

  // Requests browser to stop as soon as possible. |Run| will return as soon as
  // browser stops.
  // IMPORTANT: All pointers to HeadlessBrowserContexts and HeadlessWebContents
  // become invalid after calling this function.
  virtual void Shutdown() = 0;

 protected:
  HeadlessBrowser() {}
  virtual ~HeadlessBrowser() {}
};

// Embedding API overrides for the headless browser.
struct HEADLESS_EXPORT HeadlessBrowser::Options {
  class Builder;

  Options(Options&& options);

  Options(const Options&) = delete;
  Options& operator=(const Options&) = delete;

  ~Options();

  Options& operator=(Options&& options);

  // Command line options to be passed to browser. Initialized in constructor.
  int argc;
  raw_ptr<const char*> argv;

#if BUILDFLAG(IS_WIN)
  // Set hardware instance if available, otherwise it defaults to 0.
  HINSTANCE instance = 0;

  // Set with sandbox information. This has to be already initialized.
  raw_ptr<sandbox::SandboxInterfaceInfo> sandbox_info = nullptr;
#endif

  // Address at which DevTools should listen for connections. Disabled by
  // default.
  net::HostPortPair devtools_endpoint;

  // Enables remote debug over stdio pipes [in=3, out=4].
  bool devtools_pipe_enabled = false;

  // A single way to test whether the devtools server has been requested.
  bool DevtoolsServerEnabled();

  // Optional message pump that overrides the default. Must outlive the browser.
  raw_ptr<base::MessagePump> message_pump = nullptr;

  // Run the browser in single process mode instead of using separate renderer
  // processes as per default. Note that this also disables any sandboxing of
  // web content, which can be a security risk.
  bool single_process_mode = false;

  // Run the browser without renderer sandbox. This option can be
  // a security risk and should be used with caution.
  bool disable_sandbox = false;

  // Whether or not to enable content::ResourceScheduler. Enabled by default.
  bool enable_resource_scheduler = true;

  // Choose the GL implementation to use for rendering. A suitable
  // implementantion is selected by default. Setting this to an empty
  // string can be used to disable GL rendering (e.g., WebGL support).
  std::string gl_implementation;

  // Choose the ANGLE implementation to use for rendering.
  // Only relevant if the gl_implementation above is set to "angle".
  std::string angle_implementation;

  // Default per-context options, can be specialized on per-context basis.

  std::string product_name_and_version;
  std::string accept_language;
  std::string user_agent;

  // The ProxyConfig to use. The system proxy settings are used by default.
  std::unique_ptr<net::ProxyConfig> proxy_config;

  // Default window size. This is also used to create the window tree host and
  // as initial screen size. Defaults to 800x600.
  gfx::Size window_size;

  // Path to user data directory, where browser will look for its state.
  // If empty, default directory (where the binary is located) will be used.
  base::FilePath user_data_dir;

  // Run a browser context in an incognito mode. Enabled by default.
  bool incognito_mode = true;

  // If true, then all pop-ups and calls to window.open will fail.
  bool block_new_web_contents = false;

  // Whether or not BeginFrames will be issued over DevTools protocol
  // (experimental).
  bool enable_begin_frame_control = false;

  // Whether or not all sites should have a dedicated process.
  bool site_per_process = false;

  // Set a callback that is invoked to override WebPreferences for RenderViews
  // created within the HeadlessBrowser. Called whenever the WebPreferences of a
  // RenderView change. Executed on the browser main thread.
  //
  // WARNING: We cannot provide any guarantees about the stability of the
  // exposed WebPreferences API, so use with care.
  base::RepeatingCallback<void(blink::web_pref::WebPreferences*)>
      override_web_preferences_callback;

  // Set a callback that is invoked when a new child process is spawned or
  // forked and allows adding additional command line flags to the child
  // process's command line. Executed on the browser main thread.
  // |child_browser_context| points to the BrowserContext of the child
  // process, but will only be set if the child process is a renderer process.
  //
  // NOTE: This callback may be called on the UI or IO thread even after the
  // HeadlessBrowser has been destroyed.
  using AppendCommandLineFlagsCallback = base::RepeatingCallback<void(
      base::CommandLine* command_line,
      HeadlessBrowserContext* child_browser_context,
      const std::string& child_process_type,
      int child_process_id)>;
  AppendCommandLineFlagsCallback append_command_line_flags_callback;

  // Minidump crash reporter settings. Crash reporting is disabled by default.
  // By default crash dumps are written to the directory containing the
  // executable.
  bool enable_crash_reporter = false;
  base::FilePath crash_dumps_dir;

  // Font render hinting value to override any default settings
  gfx::FontRenderParams::Hinting font_render_hinting;

  // Reminder: when adding a new field here, do not forget to add it to
  // HeadlessBrowserContextOptions (where appropriate).
 private:
  Options(int argc, const char** argv);
};

class HEADLESS_EXPORT HeadlessBrowser::Options::Builder {
 public:
  Builder(int argc, const char** argv);
  Builder();

  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  ~Builder();

  // Browser-wide settings.

  Builder& EnableDevToolsServer(const net::HostPortPair& endpoint);
  Builder& EnableDevToolsPipe();
  Builder& SetMessagePump(base::MessagePump* pump);
  Builder& SetSingleProcessMode(bool single_process);
  Builder& SetDisableSandbox(bool disable);
  Builder& SetEnableResourceScheduler(bool enable);
  Builder& SetGLImplementation(const std::string& implementation);
  Builder& SetANGLEImplementation(const std::string& implementation);
  Builder& SetAppendCommandLineFlagsCallback(
      const Options::AppendCommandLineFlagsCallback& callback);
#if BUILDFLAG(IS_WIN)
  Builder& SetInstance(HINSTANCE hinstance);
  Builder& SetSandboxInfo(sandbox::SandboxInterfaceInfo* info);
#endif

  // Per-context settings.

  Builder& SetProductNameAndVersion(const std::string& name_and_version);
  Builder& SetAcceptLanguage(const std::string& language);
  Builder& SetEnableBeginFrameControl(bool enable);
  Builder& SetUserAgent(const std::string& agent);
  Builder& SetProxyConfig(std::unique_ptr<net::ProxyConfig> config);
  Builder& SetWindowSize(const gfx::Size& size);
  Builder& SetUserDataDir(const base::FilePath& dir);
  Builder& SetIncognitoMode(bool incognito);
  Builder& SetSitePerProcess(bool per_process);
  Builder& SetBlockNewWebContents(bool block);
  Builder& SetOverrideWebPreferencesCallback(
      base::RepeatingCallback<void(blink::web_pref::WebPreferences*)> callback);
  Builder& SetCrashReporterEnabled(bool enabled);
  Builder& SetCrashDumpsDir(const base::FilePath& dir);
  Builder& SetFontRenderHinting(gfx::FontRenderParams::Hinting hinting);

  Options Build();

 private:
  Options options_;
};

#if !BUILDFLAG(IS_WIN)
// The headless browser may need to create child processes (e.g., a renderer
// which runs web content). This is done by re-executing the parent process as
// a zygote[1] and forking each child process from that zygote.
//
// For this to work, the embedder should call RunChildProcess as soon as
// possible (i.e., before creating any threads) to pass control to the headless
// library. In a browser process this function will return immediately, but in a
// child process it will never return. For example:
//
// int main(int argc, const char** argv) {
//   headless::RunChildProcessIfNeeded(argc, argv);
//   headless::HeadlessBrowser::Options::Builder builder(argc, argv);
//   return headless::HeadlessBrowserMain(
//       builder.Build(),
//       base::OnceCallback<void(headless::HeadlessBrowser*)>());
// }
//
// [1]
// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/zygote.md
void RunChildProcessIfNeeded(int argc, const char** argv);
#else
// In Windows, the headless browser may need to create child processes. This is
// done by re-executing the parent process which may have been initialized with
// different libraries (e.g. child_dll). In this case, the embedder has to pass
// the appropiate HINSTANCE and initalization sandbox_info to properly launch
// the child process.
void RunChildProcessIfNeeded(HINSTANCE instance,
                             sandbox::SandboxInterfaceInfo* sandbox_info);
#endif  // !BUILDFLAG(IS_WIN)

// Main entry point for running the headless browser. This function constructs
// the headless browser instance, passing it to the given
// |on_browser_start_callback| callback. Note that since this function executes
// the main loop, it will only return after HeadlessBrowser::Shutdown() is
// called, returning the exit code for the process. It is not possible to
// initialize the browser again after it has been torn down.
int HeadlessBrowserMain(
    HeadlessBrowser::Options options,
    base::OnceCallback<void(HeadlessBrowser*)> on_browser_start_callback);

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
