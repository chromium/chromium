// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBNN_INTROSPECTION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_WEBNN_INTROSPECTION_MANAGER_H_

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom-forward.h"

namespace content {

// A singleton that is used to provide WebNN introspection and debugging
// capabilities across all renderer processes. It also gets additional
// introspection data from the WebNN service in the GPU process. It is used
// only by chrome://webnn-internals.
class CONTENT_EXPORT WebNNIntrospectionManager {
 public:
  // Observers will receive updates on the various introspection data received
  // from either the renderers or the WebNN service.
  class Observer : public base::CheckedObserver {
   public:
#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
    virtual void OnGraphRecorded(const mojo_base::BigBuffer& json_data) = 0;
    virtual void OnGraphRecordEnabledChanged(bool is_enabled) {}
#endif
    virtual void OnUpdateExistingContextDetails(
        const std::vector<webnn::mojom::WebNNContextIntrospectionDetailsPtr>&
            contexts_details) = 0;
    virtual void OnUpdateAvailableExecutionProvidersDetails(
        const std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>&
            available_execution_providers) = 0;
  };

  // Return the singleton instance.
  static WebNNIntrospectionManager* GetInstance();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Establish the connection with the WebNN service in the GPU process and
  // request the details of existing WebNN contexts.
  virtual void EstablishServiceConnectionAndGetExistingContextsDetails(
      base::OnceCallback<
          void(std::vector<webnn::mojom::WebNNContextIntrospectionDetailsPtr>)>
          callback) = 0;

  // Query the WebNN service in the GPU process for the details of installed
  // execution providers.
  virtual void
  EstablishServiceConnectionAndGetAvailableExecutionProvidersDetails(
      base::OnceCallback<
          void(std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>)>
          callback) = 0;

#if BUILDFLAG(IS_WIN)
  // Force the creation of the ONNX Runtime environment for introspection. This
  // will create the ORT environment if it hasn't been created yet, force
  // installation of execution providers if applicable so WebNN Internals can
  // show them.
  virtual void ForceOrtEnvironmentCreationForIntrospection(
      base::OnceCallback<
          void(std::vector<webnn::mojom::WebNNExecutionProviderDetailsPtr>)>
          callback) = 0;
#endif

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  // Enable recording of the ML graphs.
  virtual void SetMLGraphRecordEnabled(bool enabled) = 0;
  virtual bool IsMLGraphRecordEnabled() const = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBNN_INTROSPECTION_MANAGER_H_
