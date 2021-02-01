// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_H_
#define CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
class URLDataSource;
class URLDataSourceImpl;
class WebUIDataSource;

// To serve dynamic data off of chrome: URLs, implement the
// URLDataManager::DataSource interface and register your handler
// with AddDataSource. DataSources must be added on the UI thread (they are also
// deleted on the UI thread). Internally the DataSources are maintained by
// URLDataManagerBackend, see it for details.
class CONTENT_EXPORT URLDataManager : public base::SupportsUserData::Data {
 public:
  explicit URLDataManager(BrowserContext* browser_context);
  ~URLDataManager() override;

  // Adds a DataSource to the collection of data sources. This *must* be invoked
  // on the UI thread.
  //
  // If |AddDataSource| is called more than once for a particular name it will
  // release the old |DataSource|, most likely resulting in it getting deleted
  // as there are no other references to it. |DataSource| uses the
  // |DeleteOnUIThread| trait to insure that the destructor is called on the UI
  // thread. This is necessary as some |DataSource|s notably |FileIconSource|
  // and |FaviconSource|, have members that will DCHECK if they are not
  // destructed in the same thread as they are constructed (the UI thread).
  void AddDataSource(URLDataSourceImpl* source);

  void UpdateWebUIDataSource(const std::string& source_name,
                             std::unique_ptr<base::DictionaryValue> update);

  // Deletes any data sources no longer referenced. This is normally invoked
  // for you, but can be invoked to force deletion (such as during shutdown).
  static void DeleteDataSources();

  // Convenience wrapper function to add |source| to |browser_context|'s
  // |URLDataManager|. Creates a URLDataSourceImpl to wrap the given
  // source.
  static void AddDataSource(BrowserContext* browser_context,
                            std::unique_ptr<URLDataSource> source);

  // Adds a WebUI data source to |browser_context|'s |URLDataManager|.
  static void AddWebUIDataSource(BrowserContext* browser_context,
                                 WebUIDataSource* source);

  // Updates an existing WebUI data source.
  static void UpdateWebUIDataSource(
      BrowserContext* browser_context,
      const std::string& source_name,
      std::unique_ptr<base::DictionaryValue> update);

 private:
  friend class URLDataSourceImpl;
  friend struct DeleteURLDataSource;
  typedef std::vector<const URLDataSourceImpl*> URLDataSources;

  // If invoked on the UI thread the DataSource is deleted immediatlye,
  // otherwise it is added to |data_sources_| and a task is scheduled to handle
  // deletion on the UI thread. See note abouve DeleteDataSource for more info.
  static void DeleteDataSource(const URLDataSourceImpl* data_source);

  // Returns true if |data_source| is scheduled for deletion (|DeleteDataSource|
  // was invoked).
  static bool IsScheduledForDeletion(const URLDataSourceImpl* data_source);

  BrowserContext* browser_context_;

  // |data_sources_| that are no longer referenced and scheduled for deletion.
  // Protected by g_delete_lock in the .cc file.
  static URLDataSources* data_sources_;

  DISALLOW_COPY_AND_ASSIGN(URLDataManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_H_
