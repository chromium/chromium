/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SHARED_WORKER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SHARED_WORKER_H_

#include <memory>

#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/privacy_preferences.h"
#include "third_party/blink/public/mojom/net/ip_address_space.mojom-shared.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace blink {

class MessagePortChannel;
class WebString;
class WebSharedWorkerClient;
class WebURL;

// This is the interface to a SharedWorker thread.
class BLINK_EXPORT WebSharedWorker {
 public:
  virtual ~WebSharedWorker() {}

  // Instantiate a WebSharedWorker that interacts with the shared worker.
  // WebSharedWorkerClient given here should own this instance.
  static std::unique_ptr<WebSharedWorker> Create(WebSharedWorkerClient*);

  virtual void StartWorkerContext(
      const WebURL& script_url,
      const WebString& name,
      const WebString& content_security_policy,
      WebContentSecurityPolicyType,
      mojom::IPAddressSpace,
      const base::UnguessableToken& devtools_worker_token,
      PrivacyPreferences privacy_preferences,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      mojo::ScopedMessagePipeHandle content_settings_handle,
      mojo::ScopedMessagePipeHandle interface_provider) = 0;

  // Sends a connect event to the SharedWorker context.
  virtual void Connect(MessagePortChannel) = 0;

  // Invoked to shutdown the worker when there are no more associated documents.
  // This eventually deletes this instance.
  virtual void TerminateWorkerContext() = 0;

  virtual void PauseWorkerContextOnStart() = 0;
  virtual void BindDevToolsAgent(
      mojo::ScopedInterfaceEndpointHandle devtools_agent_host_ptr_info,
      mojo::ScopedInterfaceEndpointHandle devtools_agent_request) = 0;

  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;
};

}  // namespace blink

#endif
