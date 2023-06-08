// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
#define REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/management/platform_management_service.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class AutoThreadTaskRunner;

// A class that manages threads and running context for the chromoting host
// process. This class is virtual to allow for platform specialization and
// testing purposes.
class ChromotingHostContext {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Attaches task runners to the relevant browser threads for the chromoting
  // host. Must be called on the UI thread of the browser process.
  static std::unique_ptr<ChromotingHostContext> CreateForChromeOS(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
#else
  // Create threads and URLRequestContextGetter for use by a host.
  // During shutdown the caller should tear-down the ChromotingHostContext and
  // then continue to run until |ui_task_runner| is no longer referenced.
  // nullptr is returned if any threads fail to start.
  static std::unique_ptr<ChromotingHostContext> Create(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  static std::unique_ptr<ChromotingHostContext> CreateForTesting(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  ChromotingHostContext(const ChromotingHostContext&) = delete;
  ChromotingHostContext& operator=(const ChromotingHostContext&) = delete;

  virtual ~ChromotingHostContext();

  // Per-platform classes must implement these methods.
  virtual std::unique_ptr<ChromotingHostContext> Copy() = 0;
  virtual scoped_refptr<net::URLRequestContextGetter>
  url_request_context_getter() const = 0;
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  url_loader_factory() = 0;

  // Task runner for the thread that is used for the UI.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner() const;

  // Task runner for the thread used for audio capture and encoding.
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner() const;

  // Task runner for the thread that is used for blocking file
  // IO. This thread is used by the URLRequestContext to read proxy
  // configuration and by NatConfig to read policy configs.
  scoped_refptr<AutoThreadTaskRunner> file_task_runner() const;

  // Task runner for the thread that is used by the InputInjector.
  //
  // TODO(sergeyu): Do we need a separate thread for InputInjector?
  // Can we use some other thread instead?
  scoped_refptr<AutoThreadTaskRunner> input_task_runner() const;

  // Task runner for the thread used for network IO. This thread runs
  // a libjingle message loop, and is the only thread on which
  // libjingle code may be run.
  scoped_refptr<AutoThreadTaskRunner> network_task_runner() const;

  // Task runner for the thread used by the ScreenRecorder to capture
  // the screen.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner() const;

  // Task runner for the thread used to encode video streams.
  scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner() const;

  policy::ManagementService* management_service();

 protected:
  ChromotingHostContext(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
      scoped_refptr<AutoThreadTaskRunner> audio_task_runner,
      scoped_refptr<AutoThreadTaskRunner> file_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> network_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner);

 private:
  // Caller-supplied UI thread. This is usually the application main thread.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  // Thread for audio capture and encoding.
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner_;

  // Thread for I/O operations.
  scoped_refptr<AutoThreadTaskRunner> file_task_runner_;

  // Thread for input injection.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner_;

  // Thread for network operations.
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;

  // Thread for screen capture.
  scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner_;

  // Thread for video encoding.
  scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
