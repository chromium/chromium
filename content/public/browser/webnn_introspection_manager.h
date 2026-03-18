// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBNN_INTROSPECTION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_WEBNN_INTROSPECTION_MANAGER_H_

#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"

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
        const std::string& contexts_details_json) = 0;
  };

  // Return the singleton instance.
  static WebNNIntrospectionManager* GetInstance();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Establish the connection with the WebNN service in the GPU process and
  // request the details of existing WebNN contexts.
  virtual void EstablishServiceConnectionAndGetExistingContextsDetails() = 0;

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  // Enable recording of the ML graphs.
  virtual void SetMLGraphRecordEnabled(bool enabled) = 0;
  virtual bool IsMLGraphRecordEnabled() const = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBNN_INTROSPECTION_MANAGER_H_
