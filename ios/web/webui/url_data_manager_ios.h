// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_URL_DATA_MANAGER_IOS_H_
#define IOS_WEB_WEBUI_URL_DATA_MANAGER_IOS_H_

#include <string>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"

namespace web {
class BrowserState;
class URLDataSourceIOS;
class URLDataSourceIOSImpl;
class WebUIIOSDataSource;

// To serve dynamic data off of chrome: URLs, implement the
// URLDataManagerIOS::DataSource interface and register your handler
// with AddDataSource. DataSources must be added on the UI thread (they are also
// deleted on the UI thread). Internally the DataSources are maintained by
// URLDataManagerIOSBackend, see it for details.
class URLDataManagerIOS : public base::SupportsUserData::Data {
 public:
  explicit URLDataManagerIOS(BrowserState* browser_state);

  URLDataManagerIOS(const URLDataManagerIOS&) = delete;
  URLDataManagerIOS& operator=(const URLDataManagerIOS&) = delete;

  ~URLDataManagerIOS() override;

  // Adds a DataSource to the collection of data sources. This *must* be invoked
  // on the UI thread.
  //
  // If `AddDataSource` is called more than once for a particular name it will
  // release the old `DataSource`, most likely resulting in it getting deleted
  // as there are no other references to it. `DataSource` uses the
  // `DeleteOnUIThread` trait to insure that the destructor is called on the UI
  // thread. This is necessary as some `DataSource`s notably `FileIconSource`
  // and `FaviconSource`, have members that will DCHECK if they are not
  // destructed in the same thread as they are constructed (the UI thread).
  void AddDataSource(URLDataSourceIOSImpl* source);

  // Deletes any data sources no longer referenced. This is normally invoked
  // for you, but can be invoked to force deletion (such as during shutdown).
  static void DeleteDataSources();

  // Convenience wrapper function to add `source` to `browser_context`'s
  // `URLDataManagerIOS`. Creates a URLDataSourceIOSImpl to wrap the given
  // source.
  static void AddDataSource(BrowserState* browser_context,
                            URLDataSourceIOS* source);

  // Adds a WebUI data source to `browser_context`'s `URLDataManagerIOS`.
  static void AddWebUIIOSDataSource(BrowserState* browser_state,
                                    WebUIIOSDataSource* source);

 private:
  friend class URLDataSourceIOSImpl;
  friend struct DeleteURLDataSourceIOS;
  typedef std::vector<const URLDataSourceIOSImpl*> URLDataSources;

  // Invoked on the IO thread to do the actual adding of the DataSource.
  static void AddDataSourceOnIOThread(
      BrowserState* browser_state,
      scoped_refptr<URLDataSourceIOSImpl> data_source);

  // If invoked on the UI thread the DataSource is deleted immediately,
  // otherwise it is added to `data_sources_` and a task is scheduled to handle
  // deletion on the UI thread. See note above DeleteDataSource for more info.
  static void DeleteDataSource(const URLDataSourceIOSImpl* data_source);

  // Returns true if `data_source` is scheduled for deletion (`DeleteDataSource`
  // was invoked).
  static bool IsScheduledForDeletion(const URLDataSourceIOSImpl* data_source);

  raw_ptr<BrowserState> browser_state_;

  // `data_sources_` that are no longer referenced and scheduled for deletion.
  // Protected by g_delete_lock in the .cc file.
  static URLDataSources* data_sources_;
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_URL_DATA_MANAGER_IOS_H_
