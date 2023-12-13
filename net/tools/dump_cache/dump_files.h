// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_DUMP_CACHE_DUMP_FILES_H_
#define NET_TOOLS_DUMP_CACHE_DUMP_FILES_H_

// Performs basic inspection of the disk cache files with minimal disruption
// to the actual files (they still may change if an error is detected on the
// files).

#include "base/files/file_path.h"

// Check file version of the specified cache.
bool CheckFileVersion(const base::FilePath& input_path);

// Dumps all entries from the cache.
int DumpContents(const base::FilePath& input_path);

// Dumps the headers of all files.
int DumpHeaders(const base::FilePath& input_path);

// Dumps all lists of entries.
int DumpLists(const base::FilePath& input_path);

// Dumps a given entry. |at| can be the address of the entry, or the address of
// the rankings node, or just another block address to dump as data.
int DumpEntryAt(const base::FilePath& input_path, const std::string& at);

// Dumps the allocation bitmap of the given |file|.
int DumpAllocation(const base::FilePath& file);

#endif  // NET_TOOLS_DUMP_CACHE_DUMP_FILES_H_
