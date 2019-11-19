// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/download/public/common/download_item.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_manager.h"
#include "content/public/common/referrer.h"
#include "ui/base/dragdrop/download_file_interface.h"
#include "url/gurl.h"

namespace content {

class WebContents;

// This class implements downloading a file via dragging virtual files out of
// the browser:
// http://lists.whatwg.org/htdig.cgi/whatwg-whatwg.org/2009-August/022118.html
// http://www.html5rocks.com/en/tutorials/casestudies/box_dnd_download/
class CONTENT_EXPORT DragDownloadFile : public ui::DownloadFileProvider {
 public:
  // On Windows, we need to download into a temporary file. On posix, we need to
  // download into a file that has already been created, so only the UI
  // thread is involved. |file| must be null on windows but non-null on
  // posix systems. |file_path| is an absolute path on all systems.
  DragDownloadFile(const base::FilePath& file_path,
                   base::File file,
                   const GURL& url,
                   const Referrer& referrer,
                   const std::string& referrer_encoding,
                   WebContents* web_contents);
  ~DragDownloadFile() override;

  // DownloadFileProvider methods.
  void Start(ui::DownloadFileObserver* observer) override;
  bool Wait() override;
  void Stop() override;

 private:
  class DragDownloadFileUI;
  enum State {INITIALIZED, STARTED, SUCCESS, FAILURE};

  void DownloadCompleted(bool is_successful);
  void CheckThread();

  base::FilePath file_path_;
  base::File file_;
  State state_;
  scoped_refptr<ui::DownloadFileObserver> observer_;
  base::RunLoop nested_loop_;
  DragDownloadFileUI* drag_ui_;
  base::WeakPtrFactory<DragDownloadFile> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DragDownloadFile);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_FILE_H_
