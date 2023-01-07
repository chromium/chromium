// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/huffman_trie/trie/trie_writer.h"

#include <algorithm>
#include <ostream>

#include "base/check.h"
#include "net/tools/huffman_trie/trie/trie_bit_buffer.h"

namespace net::huffman_trie {

namespace {

bool CompareReversedEntries(
    const std::unique_ptr<net::huffman_trie::ReversedEntry>& lhs,
    const std::unique_ptr<net::huffman_trie::ReversedEntry>& rhs) {
  return lhs->reversed_name < rhs->reversed_name;
}

// Searches for the longest common prefix for all entries between |start| and
// |end|.
std::vector<uint8_t> LongestCommonPrefix(ReversedEntries::const_iterator start,
                                         ReversedEntries::const_iterator end) {
  if (start == end) {
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> prefix;
  for (size_t i = 0;; ++i) {
    if (i > (*start)->reversed_name.size()) {
      break;
    }

    uint8_t candidate = (*start)->reversed_name.at(i);
    if (candidate == kTerminalValue) {
      break;
    }

    bool ok = true;
    for (auto it = start + 1; it != end; ++it) {
      if (i > (*it)->reversed_name.size() ||
          (*it)->reversed_name.at(i) != candidate) {
        ok = false;
        break;
      }
    }

    if (!ok) {
      break;
    }

    prefix.push_back(candidate);
  }

  return prefix;
}

// Returns the reversed |hostname| as a vector of bytes. The reversed hostname
// will be terminated by |kTerminalValue|.
std::vector<uint8_t> ReverseName(const std::string& hostname) {
  size_t hostname_size = hostname.size();
  std::vector<uint8_t> reversed_name(hostname_size + 1);

  for (size_t i = 0; i < hostname_size; ++i) {
    reversed_name[i] = hostname[hostname_size - i - 1];
  }

  reversed_name[reversed_name.size() - 1] = kTerminalValue;
  return reversed_name;
}

// Removes the first |length| characters from all entries between |start| and
// |end|.
void RemovePrefix(size_t length,
                  ReversedEntries::iterator start,
                  ReversedEntries::iterator end) {
  for (auto it = start; it != end; ++it) {
    (*it)->reversed_name.erase((*it)->reversed_name.begin(),
                               (*it)->reversed_name.begin() + length);
  }
}

}  // namespace

ReversedEntry::ReversedEntry(std::vector<uint8_t> reversed_name,
                             const TrieEntry* entry)
    : reversed_name(reversed_name), entry(entry) {}

ReversedEntry::~ReversedEntry() = default;

TrieWriter::TrieWriter(
    const huffman_trie::HuffmanRepresentationTable& huffman_table,
    huffman_trie::HuffmanBuilder* huffman_builder)
    : huffman_table_(huffman_table), huffman_builder_(huffman_builder) {}

TrieWriter::~TrieWriter() = default;

bool TrieWriter::WriteEntries(const TrieEntries& entries,
                              uint32_t* root_position) {
  if (entries.empty())
    return false;

  ReversedEntries reversed_entries;
  for (auto* const entry : entries) {
    auto reversed_entry =
        std::make_unique<ReversedEntry>(ReverseName(entry->name()), entry);
    reversed_entries.push_back(std::move(reversed_entry));
  }

  std::stable_sort(reversed_entries.begin(), reversed_entries.end(),
                   CompareReversedEntries);

  return WriteDispatchTables(reversed_entries.begin(), reversed_entries.end(),
                             root_position);
}

bool TrieWriter::WriteDispatchTables(ReversedEntries::iterator start,
                                     ReversedEntries::iterator end,
                                     uint32_t* position) {
  DCHECK(start != end) << "No entries passed to WriteDispatchTables";

  TrieBitBuffer writer;

  std::vector<uint8_t> prefix = LongestCommonPrefix(start, end);
  writer.WriteSize(prefix.size());

  if (prefix.size()) {
    for (uint8_t c : prefix) {
      writer.WriteChar(c, huffman_table_, huffman_builder_);
    }
  }

  RemovePrefix(prefix.size(), start, end);
  int32_t last_position = -1;

  while (start != end) {
    uint8_t candidate = (*start)->reversed_name.at(0);
    auto sub_entries_end = start + 1;

    for (; sub_entries_end != end; sub_entries_end++) {
      if ((*sub_entries_end)->reversed_name.at(0) != candidate) {
        break;
      }
    }

    writer.WriteChar(candidate, huffman_table_, huffman_builder_);

    if (candidate == kTerminalValue) {
      if (sub_entries_end - start != 1) {
        return false;
      }
      if (!(*start)->entry->WriteEntry(&writer)) {
        return false;
      }
    } else {
      RemovePrefix(1, start, sub_entries_end);
      uint32_t table_position;
      if (!WriteDispatchTables(start, sub_entries_end, &table_position)) {
        return false;
      }

      writer.WritePosition(table_position, &last_position);
    }

    start = sub_entries_end;
  }

  writer.WriteChar(kEndOfTableValue, huffman_table_, huffman_builder_);

  *position = buffer_.position();
  writer.Flush();
  writer.WriteToBitWriter(&buffer_);
  return true;
}

uint32_t TrieWriter::position() const {
  return buffer_.position();
}

void TrieWriter::Flush() {
  buffer_.Flush();
}

}  // namespace net::huffman_trie
