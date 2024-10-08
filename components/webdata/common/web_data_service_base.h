// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "components/webdata/common/webdata_export.h"
#include "sql/init_status.h"

class WebDatabase;
class WebDatabaseService;

namespace base {
class SequencedTaskRunner;
}

// Base for WebDataService class hierarchy.
// WebDataServiceBase is destroyed on the UI sequence.
class WEBDATA_EXPORT WebDataServiceBase
    : public base::RefCountedDeleteOnSequence<WebDataServiceBase> {
 public:
  // All requests return an opaque handle of the following type.
  typedef int Handle;

  // Users of this class may provide a callback to handle errors
  // (e.g. by showing a UI). The callback is called only on error, and
  // takes a single parameter, the sql::InitStatus value from trying
  // to open the database.
  // TODO(joi): Should we combine this with WebDatabaseService::InitCallback?
  using ProfileErrorCallback =
      base::OnceCallback<void(sql::InitStatus, const std::string&)>;

  // |callback| will only be invoked on error, and only if
  // |callback.is_null()| evaluates to false.
  //
  // The ownership of |wdbs| is shared, with the primary owner being the
  // WebDataServiceWrapper, and secondary owners being subclasses of
  // WebDataServiceBase, which receive |wdbs| upon construction. The
  // WebDataServiceWrapper handles the initializing and shutting down and of
  // the |wdbs| object.
  // WebDataServiceBase is destroyed on the UI sequence.
  WebDataServiceBase(
      scoped_refptr<WebDatabaseService> wdbs,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner);

  WebDataServiceBase(const WebDataServiceBase&) = delete;
  WebDataServiceBase& operator=(const WebDataServiceBase&) = delete;

  // Cancel any pending request. You need to call this method if your
  // WebDataServiceConsumer is about to be deleted.
  virtual void CancelRequest(Handle h);

  // Shutdown the web data service. The service can no longer be used after this
  // call.
  virtual void ShutdownOnUISequence();

  // Initializes the web data service, invoking `callback` if there are any
  // errors.
  void Init(ProfileErrorCallback callback);

  // Unloads the database and shuts down service.
  void ShutdownDatabase();

  // Returns a pointer to the DB (used by SyncableServices). May return NULL if
  // the database is unavailable. Must be called on DB sequence.
  WebDatabase* GetDatabase();

  // Test-only API to verify if the database is stored in-memory only, as
  // opposed to on-disk storage.
  bool UsesInMemoryDatabaseForTest() const;

 protected:
  friend class base::RefCountedDeleteOnSequence<WebDataServiceBase>;
  friend class base::DeleteHelper<WebDataServiceBase>;

  virtual ~WebDataServiceBase();

  // Our database service.
  scoped_refptr<WebDatabaseService> wdbs_;
};

#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATA_SERVICE_BASE_H_
