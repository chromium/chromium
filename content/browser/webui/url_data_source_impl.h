// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_URL_DATA_SOURCE_IMPL_H_
#define CONTENT_BROWSER_WEBUI_URL_DATA_SOURCE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "content/browser/webui/url_data_manager.h"

namespace content {
class URLDataManagerBackend;
class URLDataSource;
class URLDataSourceImpl;

// Trait used to handle deleting a URLDataSource. Deletion happens on the UI
// thread.
//
// Implementation note: the normal shutdown sequence is for the UI loop to
// stop pumping events then the IO loop and thread are stopped. When the
// URLDataSources are no longer referenced (which happens when IO thread stops)
// they get added to the UI message loop for deletion. But because the UI loop
// has stopped by the time this happens the URLDataSources would be leaked.
//
// To make sure URLDataSources are properly deleted URLDataManager manages
// deletion of the URLDataSources.  When a URLDataSource is no longer referenced
// it is added to |data_sources_| and a task is posted to the UI thread to
// handle the actual deletion. During shutdown |DeleteDataSources| is invoked so
// that all pending URLDataSources are properly deleted.
struct DeleteURLDataSource {
  static void Destruct(const URLDataSourceImpl* data_source) {
    URLDataManager::DeleteDataSource(data_source);
  }
};

// A URLDataSource is an object that can answer requests for data
// asynchronously. URLDataSources are collectively owned with refcounting smart
// pointers and should never be deleted on the IO thread, since their calls
// are handled almost always on the UI thread and there's a possibility of a
// data race.  The |DeleteDataSource| trait above is used to enforce this.
class URLDataSourceImpl
    : public base::RefCountedThreadSafe<URLDataSourceImpl,
                                        DeleteURLDataSource> {
 public:
  // See |source_name_| below for docs on that parameter.
  URLDataSourceImpl(const std::string& source_name,
                    std::unique_ptr<URLDataSource> source);

  const std::string& source_name() const { return source_name_; }
  URLDataSource* source() const { return source_.get(); }

  virtual bool IsWebUIDataSourceImpl() const;

 protected:
  virtual ~URLDataSourceImpl();

 private:
  friend class URLDataManager;
  friend class URLDataManagerBackend;
  friend class base::DeleteHelper<URLDataSourceImpl>;

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  const std::string source_name_;

  // This field is set and maintained by URLDataManagerBackend. It is set when
  // the DataSource is added. A DataSource can be removed in two ways:
  // (1) The URLDataManagerBackend is deleted, and the weak ptr is invalidated.
  //     In this case queries pending against this data source will implicitly
  //     be dropped as their responses will have no backend for routing.
  // (2) Another DataSource is registered with the same name. In this case the
  //     backend still exists and remains referenced by this data source,
  //     allowing pending queries to be routed to the backend that formerly
  //     owned them.
  // This field should only be referenced on the IO thread. This reference can't
  // be via a scoped_refptr else there would be a cycle between the backend and
  // the data source.
  base::WeakPtr<URLDataManagerBackend> backend_;

  std::unique_ptr<URLDataSource> source_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_URL_DATA_SOURCE_IMPL_H_
