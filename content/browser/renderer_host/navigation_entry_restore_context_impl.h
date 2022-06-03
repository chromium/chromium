// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_ENTRY_RESTORE_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_ENTRY_RESTORE_CONTEXT_IMPL_H_

#include <map>
#include <string>

#include "content/public/browser/navigation_entry_restore_context.h"

class GURL;

namespace content {

class FrameNavigationEntry;

// NavigationEntryRestoreContextImpl is passed to
// NavigationEntry::SetPageState() when restoring a vector of NavigationEntries.
// It tracks the item sequence number (ISNs) associated with each
// FrameNavigationEntry (FNE), and maintains a mapping of ISNs to FNEs to
// ensure FNEs are de-duplicated if they appear in multiple NavigationEntries.
class CONTENT_EXPORT NavigationEntryRestoreContextImpl
    : public NavigationEntryRestoreContext {
 public:
  NavigationEntryRestoreContextImpl();
  ~NavigationEntryRestoreContextImpl() override;
  NavigationEntryRestoreContextImpl(const NavigationEntryRestoreContextImpl&) =
      delete;
  NavigationEntryRestoreContextImpl& operator=(
      const NavigationEntryRestoreContextImpl&) = delete;

  void AddFrameNavigationEntry(FrameNavigationEntry* entry);
  // If there is an existing FrameNavigationEntry with the given
  // |item_sequence_number| targeting |unique_name|, this returns it so that it
  // can be shared. Entries with the same ISN should always have the same URL,
  // so this returns null if the located FrameNavigationEntry differs from
  // |expected_url| to avoid bugs.
  FrameNavigationEntry* GetFrameNavigationEntryForItemSequenceNumber(
      int64_t item_sequence_number,
      const std::string& unique_name,
      const GURL& expected_url);

 private:
  // As an added precaution, we key based on both item sequence number and
  // the unique name of the frame, just in case a sequence number somehow
  // appears in multiple frames.
  //
  // We also ensure that entries with an item sequence number of 0 (the default
  // value) cannot be stored or retrieved, since they may not represent the same
  // document. This may happen for entries restored without all available state,
  // and we can skip FrameNavigationEntry sharing for them because they are not
  // considered same-document anyway.
  struct Key {
    Key(int64_t isn, const std::string& name)
        : item_sequence_number(isn), unique_name(name) {}
    bool operator==(const Key& other) const {
      return item_sequence_number == other.item_sequence_number &&
             unique_name == other.unique_name;
    }
    const int64_t item_sequence_number;
    const std::string unique_name;

    struct Compare {
      bool operator()(const Key& x, const Key& y) const {
        if (x.item_sequence_number != y.item_sequence_number)
          return x.item_sequence_number > y.item_sequence_number;
        return x.unique_name > y.unique_name;
      }
    };
  };
  std::map<Key, FrameNavigationEntry*, Key::Compare> entries_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_ENTRY_RESTORE_CONTEXT_IMPL_H_
