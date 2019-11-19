// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
#define REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

namespace remoting {

class AutoThreadTaskRunner;

// A class that manages threads and running context for the chromoting host
// process.  This class is virtual only for testing purposes (see below).
class ChromotingHostContext {
 public:
  // Create threads and URLRequestContextGetter for use by a host.
  // During shutdown the caller should tear-down the ChromotingHostContext and
  // then continue to run until |ui_task_runner| is no longer referenced.
  // nullptr is returned if any threads fail to start.
  static std::unique_ptr<ChromotingHostContext> Create(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner);

#if defined(OS_CHROMEOS)
  // Attaches task runners to the relevant browser threads for the chromoting
  // host.  Must be called on the UI thread of the browser process.
  // remoting::UrlRequestContextGetter returns ContainerURLRequestContext under
  // the hood which spawns two new threads per instance.  Since
  // ChromotingHostContext can be destroyed from any thread, as its owner
  // (It2MeHost) is ref-counted, joining the created threads during shutdown
  // violates the "Disallow IO" thread restrictions on some task runners (e.g.
  // the IO Thread of the browser process).
  // Instead, we re-use the |url_request_context_getter| in the browser process.
  static std::unique_ptr<ChromotingHostContext> CreateForChromeOS(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);
#endif  // defined(OS_CHROMEOS)

  ~ChromotingHostContext();

  std::unique_ptr<ChromotingHostContext> Copy();

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

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter()
      const;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory();

 private:
  ChromotingHostContext(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
      scoped_refptr<AutoThreadTaskRunner> audio_task_runner,
      scoped_refptr<AutoThreadTaskRunner> file_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> network_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

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

  // Serves URLRequestContexts that use the network and UI task runners.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // Makes a SharedURLLoaderFactory out of |url_request_context_getter_|
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHostContext);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
