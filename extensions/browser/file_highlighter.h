// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_FILE_HIGHLIGHTER_H_
#define EXTENSIONS_BROWSER_FILE_HIGHLIGHTER_H_

#include <stddef.h>

#include <string>

namespace extensions {

// The FileHighlighter class is used in order to isolate and highlight a portion
// of a given file (in string form). The Highlighter will split the source into
// three portions: the portion before the highlighted feature, the highlighted
// feature, and the portion following the highlighted feature.
// The file will be parsed for highlighting upon construction of the Highlighter
// object.
class FileHighlighter {
 public:
  FileHighlighter(const FileHighlighter&) = delete;
  FileHighlighter& operator=(const FileHighlighter&) = delete;

  virtual ~FileHighlighter();

  // Get the portion of the manifest which should not be highlighted and is
  // before the feature.
  std::string GetBeforeFeature() const;

  // Get the feature portion of the manifest, which should be highlighted.
  std::string GetFeature() const;

  // Get the portion of the manifest which should not be highlighted and is
  // after the feature.
  std::string GetAfterFeature() const;

 protected:
  explicit FileHighlighter(const std::string& contents);

  // The contents of the file we are parsing.
  std::string contents_;

  // The start of the feature.
  size_t start_;

  // The end of the feature.
  size_t end_;
};

// Use the ManifestHighlighter class to find the bounds of a feature in the
// manifest.
// A feature can be at any level in the hierarchy. The "start" of a feature is
// the first character of the feature name, or the beginning quote of the name,
// if present. The "end" of a feature is wherever the next item at the same
// level starts.
// For instance, the bounds for the 'permissions' feature at the top level could
// be '"permissions": { "tabs", "history", "downloads" }', but the feature for
// 'tabs' within 'permissions' would just be '"tabs"'.
// We can't use the JSONParser to do this, because we want to display the actual
// manifest, and once we parse it into Values, we lose any formatting the user
// may have had.
// If a feature cannot be found, the feature will have zero-length.
class ManifestHighlighter : public FileHighlighter {
 public:
  ManifestHighlighter(const std::string& manifest,
                      const std::string& key,
                      const std::string& specific /* optional */);

  ManifestHighlighter(const ManifestHighlighter&) = delete;
  ManifestHighlighter& operator=(const ManifestHighlighter&) = delete;

  ~ManifestHighlighter() override;

 private:
  // Called from the constructor; determine the start and end bounds of a
  // feature, using both the key and specific information.
  void Parse(const std::string& key, const std::string& specific);

  // Find the bounds of any feature, either a full key or a specific item within
  // the key. |enforce_at_top_level| means that the feature we find must be at
  // the same level as |start_| (i.e., ignore nested elements).
  // Returns true on success.
  bool FindBounds(const std::string& feature, bool enforce_at_top_level);

  // Finds the end of the feature.
  void FindBoundsEnd(const std::string& feature, size_t local_start);
};

// Use the SourceHighlighter to highlight a particular line in a given source
// file.
class SourceHighlighter : public FileHighlighter {
 public:
  SourceHighlighter(const std::string& source, size_t line_number);

  SourceHighlighter(const SourceHighlighter&) = delete;
  SourceHighlighter& operator=(const SourceHighlighter&) = delete;

  ~SourceHighlighter() override;

 private:
  // Called from the constructor; determine the bounds of the line in the source
  // file.
  void Parse(size_t line_number);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_FILE_HIGHLIGHTER_H_
