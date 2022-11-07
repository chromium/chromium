// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_
#define REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/telemetry_log_writer.h"

namespace base {
class SingleThreadTaskExecutor;

template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

// Houses the global resources on which the Chromoting components run
// (e.g. message loops and task runners).
namespace remoting {

class DirectoryServiceClient;

class ChromotingClientRuntime {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // RuntimeWillShutdown will be called on the delegate when the runtime
    // enters into the destructor. This is a good time for the delegate to
    // start shutting down on threads while they exist.
    virtual void RuntimeWillShutdown() = 0;

    // RuntimeDidShutdown will be called after task managers and threads
    // have been stopped.
    virtual void RuntimeDidShutdown() = 0;

    // For fetching auth token. Called on the UI thread.
    virtual base::WeakPtr<OAuthTokenGetter> oauth_token_getter() = 0;
  };

  static ChromotingClientRuntime* GetInstance();

  ChromotingClientRuntime(const ChromotingClientRuntime&) = delete;
  ChromotingClientRuntime& operator=(const ChromotingClientRuntime&) = delete;

  // Must be called before calling any other methods on this object.
  void Init(ChromotingClientRuntime::Delegate* delegate);

  std::unique_ptr<OAuthTokenGetter> CreateOAuthTokenGetter();

  base::SequenceBound<DirectoryServiceClient> CreateDirectoryServiceClient();

  scoped_refptr<AutoThreadTaskRunner> network_task_runner() {
    return network_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> audio_task_runner() {
    return audio_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> display_task_runner() {
    return display_task_runner_;
  }

  scoped_refptr<net::URLRequestContextGetter> url_requester() {
    return url_requester_;
  }

  // Must be called from the network thread.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory();

  ChromotingEventLogWriter* log_writer() { return log_writer_.get(); }

 private:
  ChromotingClientRuntime();
  virtual ~ChromotingClientRuntime();

  // Initializes URL loader factory owner, log writer, and other resources on
  // the network thread.
  void InitializeOnNetworkThread();

  // Chromium code's connection to the app message loop. Once created the
  // SingleThreadTaskExecutor will live for the life of the program.
  std::unique_ptr<base::SingleThreadTaskExecutor> ui_task_executor_;

  // References to native threads.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  // TODO(nicholss): AutoThreads will be leaked because they depend on the main
  // thread. We should update this class to use regular threads like the client
  // plugin does.
  // Longer term we should migrate most of these to background tasks except the
  // network thread to ThreadPool, removing the need for threads.

  scoped_refptr<AutoThreadTaskRunner> audio_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> display_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_requester_;
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;

  // For logging session stage changes and stats.
  std::unique_ptr<TelemetryLogWriter> log_writer_;

  raw_ptr<ChromotingClientRuntime::Delegate> delegate_ = nullptr;

  friend struct base::DefaultSingletonTraits<ChromotingClientRuntime>;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_
