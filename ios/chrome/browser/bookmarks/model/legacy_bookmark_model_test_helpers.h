// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_TEST_HELPERS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_TEST_HELPERS_H_

#include <string>

class LegacyBookmarkModel;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Blocks until `model` finishes loading.
void WaitForLegacyBookmarkModelToLoad(LegacyBookmarkModel* model);

// Return the descendants of `node` as a string useful for verifying node
// modifications. The format of the resulting string is:
//
//           result = node " " , { node " " }
//             node = bookmark title ` folder
//           folder = folder title ":[ " { node " " } "]"
//   bookmark title = (* string with no spaces *)
//     folder title = (* string with no spaces *)
//
// Example: "a f1:[ b d c ] d f2:[ e f g ] h "
//
// (Logically, we should use `string16`s, but it's more convenient for test
// purposes to use (UTF-8) `std::string`s.)
std::string LegacyBookmarkModelStringFromNode(
    const bookmarks::BookmarkNode* node);

// Create and add the node hierarchy specified by `model_string` to the
// bookmark node given by `node`. The string has the same format as
// specified for LegacyBookmarkModelStringFromNode(). The new nodes added to
// `node` are appended to the end of node's existing subnodes, if any. `model`
// must be the model of which `node` is a member. NOTE: The string format is
// very rigid and easily broken if not followed
//       exactly (since we're using a very simple parser).
void AddNodesFromLegacyBookmarkModelString(LegacyBookmarkModel* model,
                                           const bookmarks::BookmarkNode* node,
                                           const std::string& model_string);

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_LEGACY_BOOKMARK_MODEL_TEST_HELPERS_H_
