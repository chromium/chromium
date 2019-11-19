/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/filesystem/dom_file_path.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

const char DOMFilePath::kSeparator = '/';
const char DOMFilePath::kRoot[] = "/";

String DOMFilePath::Append(const String& base, const String& components) {
  return EnsureDirectoryPath(base) + components;
}

String DOMFilePath::EnsureDirectoryPath(const String& path) {
  if (!DOMFilePath::EndsWithSeparator(path))
    return path + DOMFilePath::kSeparator;
  return path;
}

String DOMFilePath::GetName(const String& path) {
  int index = path.ReverseFind(DOMFilePath::kSeparator);
  if (index != -1)
    return path.Substring(index + 1);
  return path;
}

String DOMFilePath::GetDirectory(const String& path) {
  int index = path.ReverseFind(DOMFilePath::kSeparator);
  if (!index)
    return DOMFilePath::kRoot;
  if (index != -1)
    return path.Substring(0, index);
  return ".";
}

bool DOMFilePath::IsParentOf(const String& parent, const String& may_be_child) {
  DCHECK(DOMFilePath::IsAbsolute(parent));
  DCHECK(DOMFilePath::IsAbsolute(may_be_child));
  if (parent == DOMFilePath::kRoot && may_be_child != DOMFilePath::kRoot)
    return true;
  if (parent.length() >= may_be_child.length() ||
      !may_be_child.StartsWithIgnoringCase(parent))
    return false;
  if (may_be_child[parent.length()] != DOMFilePath::kSeparator)
    return false;
  return true;
}

String DOMFilePath::RemoveExtraParentReferences(const String& path) {
  DCHECK(DOMFilePath::IsAbsolute(path));
  Vector<String> components;
  Vector<String> canonicalized;
  path.Split(DOMFilePath::kSeparator, components);
  for (const auto& component : components) {
    if (component == ".")
      continue;
    if (component == "..") {
      if (canonicalized.size() > 0)
        canonicalized.pop_back();
      continue;
    }
    canonicalized.push_back(component);
  }
  if (canonicalized.IsEmpty())
    return DOMFilePath::kRoot;
  StringBuilder result;
  for (const auto& component : canonicalized) {
    result.Append(DOMFilePath::kSeparator);
    result.Append(component);
  }
  return result.ToString();
}

bool DOMFilePath::IsValidPath(const String& path) {
  if (path.IsEmpty() || path == DOMFilePath::kRoot)
    return true;

  // Embedded NULs are not allowed.
  if (path.find(static_cast<UChar>(0)) != WTF::kNotFound)
    return false;

  // While not [yet] restricted by the spec, '\\' complicates implementation for
  // Chromium.
  if (path.find('\\') != WTF::kNotFound)
    return false;

  // This method is only called on fully-evaluated absolute paths. Any sign of
  // ".." or "." is likely an attempt to break out of the sandbox.
  Vector<String> components;
  path.Split(DOMFilePath::kSeparator, components);
  return std::none_of(components.begin(), components.end(),
                      [](const String& component) {
                        return component == "." || component == "..";
                      });
}

bool DOMFilePath::IsValidName(const String& name) {
  if (name.IsEmpty())
    return true;
  // '/' is not allowed in name.
  if (name.Contains('/'))
    return false;
  return IsValidPath(name);
}

}  // namespace blink
