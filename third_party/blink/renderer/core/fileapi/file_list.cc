/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/fileapi/file_list.h"

#include "base/files/file_path.h"
#include "third_party/blink/public/platform/file_path_conversion.h"

namespace blink {

FileList::FileList() = default;

File* FileList::item(unsigned index) const {
  if (index >= files_.size())
    return nullptr;
  return files_[index].Get();
}

Vector<base::FilePath> FileList::PathsForUserVisibleFiles() const {
  Vector<base::FilePath> paths;
  for (unsigned i = 0; i < files_.size(); ++i) {
    if (files_[i]->GetUserVisibility() == File::kIsUserVisible) {
      if (files_[i]->HasBackingFile())
        paths.push_back(StringToFilePath(files_[i]->GetPath()));
      else
        paths.push_back(StringToFilePath(files_[i]->name()));
    }
  }

  return paths;
}

void FileList::Trace(Visitor* visitor) const {
  visitor->Trace(files_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
