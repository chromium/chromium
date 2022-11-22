// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_PARSER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"

namespace media {

// Interface for receiving WebM parser events.
//
// Each method is called when an element of the specified type is parsed.
// The ID of the element that was parsed is given along with the value
// stored in the element. List elements generate calls at the start and
// end of the list. Any pointers passed to these methods are only guaranteed
// to be valid for the life of that call. Each method (except for OnListStart)
// returns a bool that indicates whether the parsed data is valid. OnListStart
// returns a pointer to a WebMParserClient object, which should be used to
// handle elements parsed out of the list being started. If false (or NULL by
// OnListStart) is returned then the parse is immediately terminated and an
// error is reported by the parser.
class MEDIA_EXPORT WebMParserClient {
 public:
  WebMParserClient(const WebMParserClient&) = delete;
  WebMParserClient& operator=(const WebMParserClient&) = delete;

  virtual ~WebMParserClient();

  virtual WebMParserClient* OnListStart(int id);
  virtual bool OnListEnd(int id);
  virtual bool OnUInt(int id, int64_t val);
  virtual bool OnFloat(int id, double val);
  virtual bool OnBinary(int id, const uint8_t* data, int size);

  // Note that |str| is not necessarily a valid WebM string-value; various EBML
  // "s" or "8" string elements are specified as either ASCII-printable (0x20 -
  // 0x7F) or UTF-8, respectively.  It is left to the overrides of this method
  // to do string format validation, if any.
  virtual bool OnString(int id, const std::string& str);

 protected:
  WebMParserClient();
};

struct ListElementInfo;

// Parses a WebM list element and all of its children. This
// class supports incremental parsing of the list so Parse()
// can be called multiple times with pieces of the list.
// IsParsingComplete() will return true once the entire list has
// been parsed.
class MEDIA_EXPORT WebMListParser {
 public:
  // |id| - Element ID of the list we intend to parse.
  // |client| - Called as different elements in the list are parsed.
  WebMListParser(int id, WebMParserClient* client);

  WebMListParser(const WebMListParser&) = delete;
  WebMListParser& operator=(const WebMListParser&) = delete;

  ~WebMListParser();

  // Resets the state of the parser so it can start parsing a new list.
  void Reset();

  // Parses list data contained in |buf|.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int Parse(const uint8_t* buf, int size);

  // Returns true if the entire list has been parsed.
  bool IsParsingComplete() const;

 private:
  enum State {
    NEED_LIST_HEADER,
    INSIDE_LIST,
    DONE_PARSING_LIST,
    PARSE_ERROR,
  };

  struct ListState {
    int id_;
    int64_t size_;
    int64_t bytes_parsed_;
    raw_ptr<const ListElementInfo> element_info_;
    raw_ptr<WebMParserClient> client_;
  };

  void ChangeState(State new_state);

  // Parses a single element in the current list.
  //
  // |header_size| - The size of the element header
  // |id| - The ID of the element being parsed.
  // |element_size| - The size of the element body.
  // |data| - Pointer to the element contents.
  // |size| - Number of bytes in |data|
  // |client| - Client to pass the parsed data to.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseListElement(int header_size,
                       int id,
                       int64_t element_size,
                       const uint8_t* data,
                       int size);

  // Called when starting to parse a new list.
  //
  // |id| - The ID of the new list.
  // |size| - The size of the new list.
  // |client| - The client object to notify that a new list is being parsed.
  //
  // Returns true if this list can be started in the current context. False
  // if starting this list causes some sort of parse error.
  bool OnListStart(int id, int64_t size);

  // Called when the end of the current list has been reached. This may also
  // signal the end of the current list's ancestors if the current list happens
  // to be at the end of its parent.
  //
  // Returns true if no errors occurred while ending this list(s).
  bool OnListEnd();

  // Checks to see if |id_b| is a sibling or ancestor of |id_a|.
  // This method is used to determine whether or not a new element |id_b| is a
  // valid continuation or valid terminator of an unknown-sized list element
  // |id_a|.  This parser doesn't allow other element types than kWebMIdCluster
  // or kWebMIdSegment to have unknown size.
  bool IsSiblingOrAncestor(int id_a, int id_b) const;

  State state_;

  // Element ID passed to the constructor.
  const int root_id_;

  // Element level for |root_id_|. Used to verify that elements appear at
  // the correct level.
  const int root_level_;

  // WebMParserClient to handle the root list.
  const raw_ptr<WebMParserClient> root_client_;

  // Stack of state for all the lists currently being parsed. Lists are
  // added and removed from this stack as they are parsed.
  std::vector<ListState> list_state_stack_;
};

// Parses an element header & returns the ID and element size.
//
// Returns < 0 if the parse fails.
// Returns 0 if more data is needed.
// Returning > 0 indicates success & the number of bytes parsed.
// |*id| contains the element ID on success and is undefined otherwise.
// |*element_size| contains the element size on success and is undefined
//                 otherwise.
int MEDIA_EXPORT WebMParseElementHeader(const uint8_t* buf,
                                        int size,
                                        int* id,
                                        int64_t* element_size);

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_PARSER_H_
