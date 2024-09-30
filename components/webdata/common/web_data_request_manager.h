// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_REQUEST_MANAGER_H__
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_REQUEST_MANAGER_H__

#include <map>
#include <memory>

#include "base/atomicops.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "components/webdata/common/web_database_service.h"

class WebDataServiceConsumer;
class WebDataRequestManager;

//////////////////////////////////////////////////////////////////////////////
//
// WebData requests
//
// Every request is processed using a request object. The object contains
// both the request parameters and the results.
//////////////////////////////////////////////////////////////////////////////
class WebDataRequest {
 public:
  WebDataRequest(const WebDataRequest&) = delete;
  WebDataRequest& operator=(const WebDataRequest&) = delete;

  virtual ~WebDataRequest();

  // Returns the identifier for this request.
  WebDataServiceBase::Handle GetHandle() const;

  // Returns |true| if the request is active and |false| if the request has been
  // cancelled or has already completed.
  bool IsActive();

 private:
  // For access to the web request mutable state under the manager's lock.
  friend class WebDataRequestManager;

  // Private constructor called for WebDataRequestManager::NewRequest.
  WebDataRequest(WebDataRequestManager* manager,
                 WebDataServiceConsumer* consumer,
                 WebDataServiceBase::Handle handle);

  // Retrieves the manager set in the constructor, if the request is still
  // active, or nullptr if the request is inactive. The returned value may
  // change between calls.
  WebDataRequestManager* GetManager();

  // Retrieves the |consumer_| set in the constructor.
  WebDataServiceConsumer* GetConsumer();

  // Retrieves the original task runner of the request.  This may be null if the
  // original task was not posted as a sequenced task.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // Marks the current request as inactive, either due to cancellation or
  // completion.
  void MarkAsInactive();

  // Tracks task runner that the request originated on.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The manager associated with this request. This is stored as a raw (untyped)
  // pointer value because it does double duty as the flag indicating whether or
  // not this request is active (non-nullptr => active).
  base::subtle::AtomicWord atomic_manager_;

  // The originator of the service request.
  base::WeakPtr<WebDataServiceConsumer> consumer_;

  // Identifier for this request.
  const WebDataServiceBase::Handle handle_;
};

//////////////////////////////////////////////////////////////////////////////
//
// WebData Request Manager
//
// Tracks all WebDataRequests for a WebDataService.
//
// Note: This is an internal interface, not to be used outside of webdata/
//////////////////////////////////////////////////////////////////////////////
class WebDataRequestManager
    : public base::RefCountedThreadSafe<WebDataRequestManager> {
 public:
  WebDataRequestManager();

  WebDataRequestManager(const WebDataRequestManager&) = delete;
  WebDataRequestManager& operator=(const WebDataRequestManager&) = delete;

  // Factory function to create a new WebDataRequest.
  // Retrieves a WeakPtr to the |consumer| so that |consumer| does not have to
  // outlive the WebDataRequestManager.
  std::unique_ptr<WebDataRequest> NewRequest(WebDataServiceConsumer* consumer);

  // Cancel any pending request.
  void CancelRequest(WebDataServiceBase::Handle h);

  // Invoked by the WebDataService when |request| has been completed.
  void RequestCompleted(std::unique_ptr<WebDataRequest> request,
                        std::unique_ptr<WDTypedResult> result);

 private:
  friend class base::RefCountedThreadSafe<WebDataRequestManager>;

  ~WebDataRequestManager();

  // This will notify the consumer in whatever thread was used to create this
  // request.
  void RequestCompletedOnThread(std::unique_ptr<WebDataRequest> request,
                                std::unique_ptr<WDTypedResult> result);

  // A lock to protect pending requests and next request handle.
  base::Lock pending_lock_;

  // Next handle to be used for requests. Incremented for each use.
  WebDataServiceBase::Handle next_request_handle_;

  std::map<WebDataServiceBase::Handle, raw_ptr<WebDataRequest, CtnExperimental>>
      pending_requests_;
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_REQUEST_MANAGER_H__
