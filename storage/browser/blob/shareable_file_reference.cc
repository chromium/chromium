// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/shareable_file_reference.h"

#include <map>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/task_runner.h"

namespace storage {

namespace {

// A shareable file map with enforcement of sequence checker.
class ShareableFileMap {
 public:
  using FileMap = std::map<base::FilePath, ShareableFileReference*>;
  using iterator = FileMap::iterator;
  using key_type = FileMap::key_type;
  using value_type = FileMap::value_type;

  ShareableFileMap() = default;

  ~ShareableFileMap() = default;

  iterator Find(key_type key) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return file_map_.find(key);
  }

  iterator End() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return file_map_.end();
  }

  std::pair<iterator, bool> Insert(value_type value) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return file_map_.insert(value);
  }

  void Erase(key_type key) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    file_map_.erase(key);
  }

#if DCHECK_IS_ON()
  void AssertCalledOnValidSequence() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
#endif  // DCHECK_IS_ON()

 private:
  FileMap file_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ShareableFileMap);
};

base::LazyInstance<ShareableFileMap>::DestructorAtExit g_file_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::Get(
    const base::FilePath& path) {
  auto found = g_file_map.Get().Find(path);
  ShareableFileReference* reference =
      (found == g_file_map.Get().End()) ? nullptr : found->second;
  return scoped_refptr<ShareableFileReference>(reference);
}

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::GetOrCreate(
    const base::FilePath& path,
    FinalReleasePolicy policy,
    base::TaskRunner* file_task_runner) {
  return GetOrCreate(
      ScopedFile(path, static_cast<ScopedFile::ScopeOutPolicy>(policy),
                 file_task_runner));
}

// static
scoped_refptr<ShareableFileReference> ShareableFileReference::GetOrCreate(
    ScopedFile scoped_file) {
  if (scoped_file.path().empty())
    return scoped_refptr<ShareableFileReference>();

  auto result = g_file_map.Get().Insert(
      ShareableFileMap::value_type(scoped_file.path(), nullptr));

  DVLOG(1) << "ShareableFileReference::GetOrCreate("
           << scoped_file.path().value() << ", "
           << (scoped_file.policy() == ScopedFile::DELETE_ON_SCOPE_OUT
                   ? "DELETE_ON_SCOPE_OUT"
                   : "DONT_DELETE_ON_SCOPE_OUT")
           << "): " << (result.second ? "Creation." : "New Reference.");

  if (result.second == false) {
    scoped_file.Release();
    return scoped_refptr<ShareableFileReference>(result.first->second);
  }

  // Wasn't in the map, create a new reference and store the pointer.
  scoped_refptr<ShareableFileReference> reference(
      new ShareableFileReference(std::move(scoped_file)));
  result.first->second = reference.get();
  return reference;
}

void ShareableFileReference::AddFinalReleaseCallback(
    FinalReleaseCallback callback) {
#if DCHECK_IS_ON()
  g_file_map.Get().AssertCalledOnValidSequence();
#endif  // DCHECK_IS_ON()
  scoped_file_.AddScopeOutCallback(std::move(callback), nullptr);
}

ShareableFileReference::ShareableFileReference(ScopedFile scoped_file)
    : scoped_file_(std::move(scoped_file)) {
  DCHECK(g_file_map.Get().Find(path())->second == nullptr);
}

ShareableFileReference::~ShareableFileReference() {
  DCHECK(g_file_map.Get().Find(path())->second == this);
  g_file_map.Get().Erase(path());
}

}  // namespace storage
