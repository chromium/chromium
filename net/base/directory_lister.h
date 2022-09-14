// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DIRECTORY_LISTER_H_
#define NET_BASE_DIRECTORY_LISTER_H_

#include <memory>
#include <vector>

#include "base/atomicops.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {

// This class provides an API for asynchronously listing the contents of a
// directory on the filesystem.  It runs a task on a background thread, and
// enumerates all files in the specified directory on that thread.  Destroying
// the lister cancels the list operation.  The DirectoryLister must only be
// used on a thread with a MessageLoop.
class NET_EXPORT DirectoryLister  {
 public:
  // Represents one file found.
  struct DirectoryListerData {
    base::FileEnumerator::FileInfo info;
    base::FilePath path;
    base::FilePath absolute_path;
  };

  // Implement this class to receive directory entries.
  class DirectoryListerDelegate {
   public:
    // Called for each file found by the lister.
    virtual void OnListFile(const DirectoryListerData& data) = 0;

    // Called when the listing is complete.
    virtual void OnListDone(int error) = 0;

   protected:
    virtual ~DirectoryListerDelegate() = default;
  };

  // Listing options
  // ALPHA_DIRS_FIRST is the default listing type:
  //   directories first in name order, then files by name order
  // Listing is recursive only if listing type is NO_SORT_RECURSIVE.
  // TODO(mmenke): Improve testing.
  enum ListingType {
    NO_SORT,
    NO_SORT_RECURSIVE,
    ALPHA_DIRS_FIRST,
  };

  DirectoryLister(const base::FilePath& dir,
                  DirectoryListerDelegate* delegate);

  DirectoryLister(const base::FilePath& dir,
                  ListingType type,
                  DirectoryListerDelegate* delegate);

  DirectoryLister(const DirectoryLister&) = delete;
  DirectoryLister& operator=(const DirectoryLister&) = delete;

  // Will invoke Cancel().
  ~DirectoryLister();

  // Call this method to start the asynchronous directory enumeration.
  void Start();

  // Call this method to asynchronously stop directory enumeration.  The
  // delegate will not be called back.
  void Cancel();

 private:
  typedef std::vector<DirectoryListerData> DirectoryList;

  // Class responsible for retrieving and sorting the actual directory list on
  // a worker pool thread. Created on the DirectoryLister's thread. As it's
  // refcounted, it's destroyed when the final reference is released, which may
  // happen on either thread.
  //
  // It's kept alive during the calls to Start() and DoneOnOriginSequence() by
  // the reference owned by the callback itself.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(const base::FilePath& dir, ListingType type, DirectoryLister* lister);
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    // May only be called on a worker pool thread.
    void Start();

    // Must be called on the origin thread.
    void CancelOnOriginSequence();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    class DataEvent;

    ~Core();

    // Called on both threads.
    bool IsCancelled() const;

    // Called on origin thread.
    void DoneOnOriginSequence(std::unique_ptr<DirectoryList> directory_list,
                              int error) const;

    const base::FilePath dir_;
    const ListingType type_;
    const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;

    // Only used on the origin thread.
    raw_ptr<DirectoryLister> lister_;

    // Set to 1 on cancellation. Used both to abort listing files early on the
    // worker pool thread for performance reasons and to ensure |lister_| isn't
    // called after cancellation on the origin thread.
    base::subtle::Atomic32 cancelled_ = 0;
  };

  // Call into the corresponding DirectoryListerDelegate. Must not be called
  // after cancellation.
  void OnListFile(const DirectoryListerData& data);
  void OnListDone(int error);

  scoped_refptr<Core> core_;
  const raw_ptr<DirectoryListerDelegate> delegate_;
};

}  // namespace net

#endif  // NET_BASE_DIRECTORY_LISTER_H_
