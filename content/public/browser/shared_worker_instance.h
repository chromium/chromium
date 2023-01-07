// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SHARED_WORKER_INSTANCE_H_
#define CONTENT_PUBLIC_BROWSER_SHARED_WORKER_INSTANCE_H_

#include <string>

#include "content/common/content_export.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/script/script_type.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_creation_context_type.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// This class hold the necessary information to decide if a shared worker
// connection request (SharedWorkerConnector::Connect()) matches an existing
// shared worker.
//
// Note: There exist one SharedWorkerInstance per SharedWorkerHost but it's
// possible to have 2 distinct SharedWorkerHost that have an identical
// SharedWorkerInstance. An example is if |url_| or |constructor_origin| has a
// "file:" scheme, which is treated as opaque.
class CONTENT_EXPORT SharedWorkerInstance {
 public:
  SharedWorkerInstance(
      const GURL& url,
      blink::mojom::ScriptType script_type,
      network::mojom::CredentialsMode credentials_mode,
      const std::string& name,
      const blink::StorageKey& storage_key,
      blink::mojom::SharedWorkerCreationContextType creation_context_type);
  SharedWorkerInstance(const SharedWorkerInstance& other);
  SharedWorkerInstance(SharedWorkerInstance&& other);
  SharedWorkerInstance& operator=(const SharedWorkerInstance& other) = delete;
  SharedWorkerInstance& operator=(SharedWorkerInstance&& other) = delete;
  ~SharedWorkerInstance();

  // Checks if this SharedWorkerInstance matches the passed url, name, and
  // constructor origin params according to the SharedWorker constructor steps
  // in the HTML spec:
  // https://html.spec.whatwg.org/multipage/workers.html#shared-workers-and-the-sharedworker-interface
  // Note that we are using StorageKey to represent the constructor origin.
  bool Matches(const GURL& url,
               const std::string& name,
               const blink::StorageKey& storage_key) const;

  // Accessors.
  const GURL& url() const { return url_; }
  const std::string& name() const { return name_; }
  blink::mojom::ScriptType script_type() const { return script_type_; }
  network::mojom::CredentialsMode credentials_mode() const {
    return credentials_mode_;
  }
  const blink::StorageKey& storage_key() const { return storage_key_; }
  blink::mojom::SharedWorkerCreationContextType creation_context_type() const {
    return creation_context_type_;
  }

 private:
  const GURL url_;
  const blink::mojom::ScriptType script_type_;

  // Used for fetching the top-level worker script.
  const network::mojom::CredentialsMode credentials_mode_;

  const std::string name_;

  // The storage key. The key contains the origin of the document that created
  // this shared worker instance. Used for security checks. See Matches() for
  // details.
  // https://html.spec.whatwg.org/multipage/workers.html#concept-sharedworkerglobalscope-constructor-origin
  const blink::StorageKey storage_key_;

  const blink::mojom::SharedWorkerCreationContextType creation_context_type_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SHARED_WORKER_INSTANCE_H_
