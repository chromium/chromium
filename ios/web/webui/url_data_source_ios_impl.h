// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_URL_DATA_SOURCE_IOS_IMPL_H_
#define IOS_WEB_WEBUI_URL_DATA_SOURCE_IOS_IMPL_H_

#include <memory>

#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "ios/web/webui/url_data_manager_ios.h"
#include "ui/base/template_expressions.h"

namespace base {
class RefCountedMemory;
}

namespace web {
class URLDataManagerIOSBackend;
class URLDataSourceIOS;
class URLDataSourceIOSImpl;

// Trait used to handle deleting a URLDataSourceIOS. Deletion happens on the UI
// thread.
//
// Implementation note: the normal shutdown sequence is for the UI loop to stop
// pumping events then the IO loop and thread are stopped. When the
// URLDataSourceIOSs are no longer referenced (which happens when IO thread
// stops) they get added to the UI message loop for deletion. But because the
// UI loop has stopped by the time this happens the URLDataSourceIOSs would be
// leaked.
//
// To make sure URLDataSourceIOSs are properly deleted URLDataManagerIOS
// manages deletion of the URLDataSourceIOSs.  When a URLDataSourceIOS is no
// longer referenced it is added to `data_sources_` and a task is posted to the
// UI thread to handle the actual deletion. During shutdown `DeleteDataSources`
// is invoked so that all pending URLDataSourceIOSs are properly deleted.
struct DeleteURLDataSourceIOS {
  static void Destruct(const URLDataSourceIOSImpl* data_source) {
    URLDataManagerIOS::DeleteDataSource(data_source);
  }
};

// A URLDataSourceIOS is an object that can answer requests for data
// asynchronously. URLDataSourceIOSs are collectively owned with refcounting
// smart pointers and should never be deleted on the IO thread, since their
// calls are handled almost always on the UI thread and there's a possibility
// of a data race.  The `DeleteDataSource` trait above is used to enforce this.
class URLDataSourceIOSImpl
    : public base::RefCountedThreadSafe<URLDataSourceIOSImpl,
                                        DeleteURLDataSourceIOS> {
 public:
  // See source_name_ below for docs on that parameter. Takes ownership of
  // `source`.
  URLDataSourceIOSImpl(const std::string& source_name,
                       URLDataSourceIOS* source);

  // Report that a request has resulted in the data `bytes`.
  // If the request can't be satisfied, pass NULL for `bytes` to indicate
  // the request is over.
  virtual void SendResponse(int request_id,
                            scoped_refptr<base::RefCountedMemory> bytes);

  const std::string& source_name() const { return source_name_; }
  URLDataSourceIOS* source() const { return source_.get(); }

  // Replacements for i18n or null if no replacements are desired.
  virtual const ui::TemplateReplacements* GetReplacements() const;

  // Whether to perform i18n replacements in JS files (needed by WebUIs that are
  // using Web Components).
  virtual bool ShouldReplaceI18nInJS() const;

 protected:
  virtual ~URLDataSourceIOSImpl();

 private:
  friend class URLDataManagerIOS;
  friend class URLDataManagerIOSBackend;
  friend class base::DeleteHelper<URLDataSourceIOSImpl>;

  // SendResponse invokes this on the IO thread. Notifies the backend to
  // handle the actual work of sending the data.
  virtual void SendResponseOnIOThread(
      int request_id,
      scoped_refptr<base::RefCountedMemory> bytes);

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  const std::string source_name_;

  // This field is set and maintained by URLDataManagerIOSBackend. It is set
  // when the DataSource is added, and unset if the DataSource is removed. A
  // DataSource can be removed in two ways: the URLDataManagerIOSBackend is
  // deleted, or another DataSource is registered with the same name. backend_
  // should only be accessed on the IO thread. This reference can't be via a
  // scoped_refptr else there would be a cycle between the backend and data
  // source.
  raw_ptr<URLDataManagerIOSBackend> backend_;

  std::unique_ptr<URLDataSourceIOS> source_;
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_URL_DATA_SOURCE_IOS_IMPL_H_
