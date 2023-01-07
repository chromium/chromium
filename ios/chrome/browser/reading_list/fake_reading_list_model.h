// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_FAKE_READING_LIST_MODEL_H_
#define IOS_CHROME_BROWSER_READING_LIST_FAKE_READING_LIST_MODEL_H_

#include "components/reading_list/core/reading_list_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// A simple implementation of ReadingListModel with minimal function
// implementation. Add implementation as needed for new tests.
class FakeReadingListModel : public ReadingListModel {
 public:
  FakeReadingListModel();
  ~FakeReadingListModel() override;
  bool loaded() const override;

  syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() override;

  const std::vector<GURL> Keys() const override;

  size_t size() const override;

  size_t unread_size() const override;

  size_t unseen_size() const override;

  void MarkAllSeen() override;

  bool DeleteAllEntries() override;

  bool GetLocalUnseenFlag() const override;

  void ResetLocalUnseenFlag() override;

  const ReadingListEntry* GetEntryByURL(const GURL& gurl) const override;

  const ReadingListEntry* GetFirstUnreadEntry(bool distilled) const override;

  bool IsUrlSupported(const GURL& url) override;

  const ReadingListEntry& AddEntry(const GURL& url,
                                   const std::string& title,
                                   reading_list::EntrySource source) override;

  const ReadingListEntry& AddEntry(
      const GURL& url,
      const std::string& title,
      reading_list::EntrySource source,
      base::TimeDelta estimated_read_time) override;

  void RemoveEntryByURL(const GURL& url) override;

  void SetReadStatus(const GURL& url, bool read) override;

  void SetEntryTitle(const GURL& url, const std::string& title) override;

  void SetEntryDistilledState(
      const GURL& url,
      ReadingListEntry::DistillationState state) override;

  void SetEstimatedReadTime(const GURL& url,
                            base::TimeDelta estimated_read_time) override;

  void SetEntryDistilledInfo(const GURL& url,
                             const base::FilePath& distilled_path,
                             const GURL& distilled_url,
                             int64_t distilation_size,
                             const base::Time& distilation_time) override;

  void SetContentSuggestionsExtra(
      const GURL& url,
      const reading_list::ContentSuggestionsExtra& extra) override;

  void SetEntry(ReadingListEntry entry);
  void SetLoaded();

  const ReadingListEntry* entry();

 private:
  absl::optional<ReadingListEntry> entry_;
  bool loaded_ = false;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_FAKE_READING_LIST_MODEL_H_
