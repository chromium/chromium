/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/html/track/vtt/vtt_parser.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/track/text_track.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_region.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_scanner.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/segmented_string.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

const unsigned kFileIdentifierLength = 6;
const unsigned kRegionIdentifierLength = 6;
const unsigned kStyleIdentifierLength = 5;

bool VTTParser::ParsePercentageValue(VTTScanner& value_scanner,
                                     double& percentage) {
  double number;
  if (!value_scanner.ScanDouble(number))
    return false;
  // '%' must be present and at the end of the setting value.
  if (!value_scanner.Scan('%'))
    return false;
  if (number < 0 || number > 100)
    return false;
  percentage = number;
  return true;
}

bool VTTParser::ParsePercentageValuePair(VTTScanner& value_scanner,
                                         char delimiter,
                                         gfx::PointF& value_pair) {
  double first_coord;
  if (!ParsePercentageValue(value_scanner, first_coord))
    return false;

  if (!value_scanner.Scan(delimiter))
    return false;

  double second_coord;
  if (!ParsePercentageValue(value_scanner, second_coord))
    return false;

  value_pair = gfx::PointF(first_coord, second_coord);
  return true;
}

VTTParser::VTTParser(VTTParserClient* client, Document& document)
    : document_(&document),
      state_(kInitial),
      decoder_(std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
          TextResourceDecoderOptions::kPlainTextContent,
          UTF8Encoding()))),
      current_start_time_(0),
      current_end_time_(0),
      current_region_(nullptr),
      client_(client),
      contains_style_block_(false) {
  UseCounter::Count(document, WebFeature::kVTTCueParser);
}

void VTTParser::GetNewCues(HeapVector<Member<TextTrackCue>>& output_cues) {
  DCHECK(output_cues.empty());
  output_cues.swap(cue_list_);
}

void VTTParser::GetNewStyleSheets(
    HeapVector<Member<CSSStyleSheet>>& output_sheets) {
  DCHECK(output_sheets.empty());
  output_sheets.swap(style_sheets_);
}

void VTTParser::ParseBytes(base::span<const char> data) {
  String text_data = decoder_->Decode(data);
  line_reader_.Append(text_data);
  Parse();
}

void VTTParser::Flush() {
  String text_data = decoder_->Flush();
  line_reader_.Append(text_data);
  line_reader_.SetEndOfStream();
  Parse();
  FlushPendingCue();
  region_map_.clear();

  base::UmaHistogramBoolean("Accessibility.VTTContainsStyleBlock",
                            contains_style_block_);
}

void VTTParser::Parse() {
  // WebVTT parser algorithm. (5.1 WebVTT file parsing.)
  // Steps 1 - 3 - Initial setup.

  String line;
  while (line_reader_.GetLine(line)) {
    switch (state_) {
      case kInitial:
        // Steps 4 - 9 - Check for a valid WebVTT signature.
        if (!HasRequiredFileIdentifier(line)) {
          if (client_)
            client_->FileFailedToParse();
          return;
        }

        state_ = kHeader;
        break;

      case kHeader:
        // Steps 11 - 14 - Collect WebVTT block
        state_ = CollectWebVTTBlock(line);
        break;

      case kRegion:
        // Collect Region settings
        state_ = CollectRegionSettings(line);
        break;

      case kStyle:
        // Collect style sheet
        state_ = CollectStyleSheet(line);
        break;

      case kId:
        // Steps 17 - 20 - Allow any number of line terminators, then initialize
        // new cue values.
        if (line.empty())
          break;

        // Step 21 - Cue creation (start a new cue).
        ResetCueValues();

        // Steps 22 - 25 - Check if this line contains an optional identifier or
        // timing data.
        state_ = CollectCueId(line);
        break;

      case kTimingsAndSettings:
        // Steps 26 - 27 - Discard current cue if the line is empty.
        if (line.empty()) {
          state_ = kId;
          break;
        }

        // Steps 28 - 29 - Collect cue timings and settings.
        state_ = CollectTimingsAndSettings(line);
        break;

      case kCueText:
        // Steps 31 - 41 - Collect the cue text, create a cue, and add it to the
        // output.
        state_ = CollectCueText(line);
        break;

      case kBadCue:
        // Steps 42 - 48 - Discard lines until an empty line or a potential
        // timing line is seen.
        state_ = IgnoreBadCue(line);
        break;
    }
  }
}

void VTTParser::FlushPendingCue() {
  DCHECK(line_reader_.IsAtEndOfStream());
  // If we're in the CueText state when we run out of data, we emit the pending
  // cue.
  if (state_ == kCueText)
    CreateNewCue();
}

bool VTTParser::HasRequiredFileIdentifier(const String& line) {
  // WebVTT parser algorithm step 6:
  // If input is more than six characters long but the first six characters
  // do not exactly equal "WEBVTT", or the seventh character is not a U+0020
  // SPACE character, a U+0009 CHARACTER TABULATION (tab) character, or a
  // U+000A LINE FEED (LF) character, then abort these steps.
  if (!line.StartsWith("WEBVTT"))
    return false;
  if (line.length() > kFileIdentifierLength) {
    UChar maybe_separator = line[kFileIdentifierLength];
    // The line reader handles the line break characters, so we don't need
    // to check for LF here.
    if (maybe_separator != kSpaceCharacter &&
        maybe_separator != kTabulationCharacter)
      return false;
  }
  return true;
}

VTTParser::ParseState VTTParser::CollectRegionSettings(const String& line) {
  // End of region block
  if (CheckAndStoreRegion(line))
    return CheckAndRecoverCue(line);

  current_region_->SetRegionSettings(line);
  return kRegion;
}

VTTParser::ParseState VTTParser::CollectStyleSheet(const String& line) {
  if (line.empty() || line.Contains("-->")) {
    auto* parser_context = MakeGarbageCollected<CSSParserContext>(
        *document_, NullURL(), true /* origin_clean */, Referrer(),
        UTF8Encoding(), ResourceFetchRestriction::kOnlyDataUrls);
    auto* style_sheet_contents =
        MakeGarbageCollected<StyleSheetContents>(parser_context);
    CSSParser::ParseSheet(
        parser_context, style_sheet_contents, current_content_.ToString(),
        CSSDeferPropertyParsing::kNo, false /* allow_import_rules */);
    auto* style_sheet =
        MakeGarbageCollected<CSSStyleSheet>(style_sheet_contents);
    style_sheet->SetConstructorDocument(*document_);
    style_sheet->SetTitle("");
    style_sheets_.push_back(style_sheet);

    return CheckAndRecoverCue(line);
  }

  if (!current_content_.empty())
    current_content_.Append('\n');
  current_content_.Append(line);

  return kStyle;
}

VTTParser::ParseState VTTParser::CollectWebVTTBlock(const String& line) {
  // collect a WebVTT block parsing. (WebVTT parser algorithm step 14)

  if (!previous_line_.Contains("-->")) {
    // If Region support is enabled.
    if (RuntimeEnabledFeatures::WebVTTRegionsEnabled() &&
        CheckAndCreateRegion(line))
      return kRegion;

    // line starts with the substring "STYLE" and remaining characters
    // zero or more U+0020 SPACE characters or U+0009 CHARACTER TABULATION
    // (tab) characters expected other than these characters it is invalid.
    if (line.StartsWith("STYLE") && StringView(line, kStyleIdentifierLength)
                                        .IsAllSpecialCharacters<IsASpace>()) {
      contains_style_block_ = true;
      current_content_.Clear();
      return kStyle;
    }
  }

  // Handle cue block.
  ParseState state = CheckAndRecoverCue(line);
  if (state != kHeader) {
    if (!previous_line_.empty() && !previous_line_.Contains("-->"))
      current_id_ = AtomicString(previous_line_);

    return state;
  }

  // store previous line for cue id.
  // length is more than 1 line clear previous_line_ and ignore line.
  if (previous_line_.empty())
    previous_line_ = line;
  else
    previous_line_ = g_empty_string;
  return state;
}

VTTParser::ParseState VTTParser::CheckAndRecoverCue(const String& line) {
  // parse cue timings and settings
  if (line.Contains("-->")) {
    ParseState state = RecoverCue(line);
    if (state != kBadCue) {
      return state;
    }
  }
  return kHeader;
}

bool VTTParser::CheckAndCreateRegion(const String& line) {
  // line starts with the substring "REGION" and remaining characters
  // zero or more U+0020 SPACE characters or U+0009 CHARACTER TABULATION
  // (tab) characters expected other than these characters it is invalid.
  if (line.StartsWith("REGION") && StringView(line, kRegionIdentifierLength)
                                       .IsAllSpecialCharacters<IsASpace>()) {
    current_region_ = VTTRegion::Create(*document_);
    return true;
  }
  return false;
}

bool VTTParser::CheckAndStoreRegion(const String& line) {
  if (!line.empty() && !line.Contains("-->"))
    return false;

  if (!current_region_->id().empty())
    region_map_.Set(current_region_->id(), current_region_);
  current_region_ = nullptr;
  return true;
}

VTTParser::ParseState VTTParser::CollectCueId(const String& line) {
  if (line.Contains("-->"))
    return CollectTimingsAndSettings(line);
  current_id_ = AtomicString(line);
  return kTimingsAndSettings;
}

VTTParser::ParseState VTTParser::CollectTimingsAndSettings(const String& line) {
  VTTScanner input(line);

  // Collect WebVTT cue timings and settings. (5.3 WebVTT cue timings and
  // settings parsing.)
  // Steps 1 - 3 - Let input be the string being parsed and position be a
  // pointer into input.
  input.SkipWhile<IsASpace>();

  // Steps 4 - 5 - Collect a WebVTT timestamp. If that fails, then abort and
  // return failure. Otherwise, let cue's text track cue start time be the
  // collected time.
  if (!CollectTimeStamp(input, current_start_time_))
    return kBadCue;
  input.SkipWhile<IsASpace>();

  // Steps 6 - 9 - If the next three characters are not "-->", abort and return
  // failure.
  if (!input.Scan("-->"))
    return kBadCue;
  input.SkipWhile<IsASpace>();

  // Steps 10 - 11 - Collect a WebVTT timestamp. If that fails, then abort and
  // return failure. Otherwise, let cue's text track cue end time be the
  // collected time.
  if (!CollectTimeStamp(input, current_end_time_))
    return kBadCue;
  input.SkipWhile<IsASpace>();

  // Step 12 - Parse the WebVTT settings for the cue (conducted in
  // TextTrackCue).
  current_settings_ = input.RestOfInputAsString();
  return kCueText;
}

VTTParser::ParseState VTTParser::CollectCueText(const String& line) {
  // Step 34.
  if (line.empty()) {
    CreateNewCue();
    return kId;
  }
  // Step 35.
  if (line.Contains("-->")) {
    // Step 39-40.
    CreateNewCue();

    // Step 41 - New iteration of the cue loop.
    return RecoverCue(line);
  }
  if (!current_content_.empty())
    current_content_.Append('\n');
  current_content_.Append(line);

  return kCueText;
}

VTTParser::ParseState VTTParser::RecoverCue(const String& line) {
  // Step 17 and 21.
  ResetCueValues();

  // Step 22.
  return CollectTimingsAndSettings(line);
}

VTTParser::ParseState VTTParser::IgnoreBadCue(const String& line) {
  if (line.empty())
    return kId;
  if (line.Contains("-->"))
    return RecoverCue(line);
  return kBadCue;
}

// A helper class for the construction of a "cue fragment" from the cue text.
class VTTTreeBuilder {
  STACK_ALLOCATED();

 public:
  explicit VTTTreeBuilder(Document& document, TextTrack* track)
      : document_(&document), track_(track) {}

  DocumentFragment* BuildFromString(const String& cue_text);

 private:
  void ConstructTreeFromToken(Document&);
  Document& GetDocument() const { return *document_; }

  VTTToken token_;
  ContainerNode* current_node_ = nullptr;
  Vector<AtomicString> language_stack_;
  Document* document_;
  TextTrack* track_;
};

DocumentFragment* VTTTreeBuilder::BuildFromString(const String& cue_text) {
  // Cue text processing based on
  // 5.4 WebVTT cue text parsing rules, and
  // 5.5 WebVTT cue text DOM construction rules

  DocumentFragment* fragment = DocumentFragment::Create(GetDocument());

  if (cue_text.empty()) {
    fragment->ParserAppendChild(Text::Create(GetDocument(), ""));
    return fragment;
  }

  current_node_ = fragment;

  VTTTokenizer tokenizer(cue_text);
  language_stack_.clear();

  while (tokenizer.NextToken(token_))
    ConstructTreeFromToken(GetDocument());

  return fragment;
}

DocumentFragment* VTTParser::CreateDocumentFragmentFromCueText(
    Document& document,
    const String& cue_text,
    TextTrack* track) {
  VTTTreeBuilder tree_builder(document, track);
  return tree_builder.BuildFromString(cue_text);
}

void VTTParser::CreateNewCue() {
  VTTCue* cue = VTTCue::Create(*document_, current_start_time_,
                               current_end_time_, current_content_.ToString());
  cue->setId(current_id_);
  cue->ParseSettings(&region_map_, current_settings_);

  cue_list_.push_back(cue);
  if (client_)
    client_->NewCuesParsed();
}

void VTTParser::ResetCueValues() {
  current_id_ = g_empty_atom;
  current_settings_ = g_empty_string;
  current_start_time_ = 0;
  current_end_time_ = 0;
  current_content_.Clear();
}

bool VTTParser::CollectTimeStamp(const String& line, double& time_stamp) {
  VTTScanner input(line);
  return CollectTimeStamp(input, time_stamp);
}

static String SerializeTimeStamp(double time_stamp) {
  uint64_t value = ClampTo<uint64_t>(time_stamp * 1000);
  unsigned milliseconds = value % 1000;
  value /= 1000;
  unsigned seconds = value % 60;
  value /= 60;
  unsigned minutes = value % 60;
  unsigned hours = static_cast<unsigned>(value / 60);
  return String::Format("%02u:%02u:%02u.%03u", hours, minutes, seconds,
                        milliseconds);
}

bool VTTParser::CollectTimeStamp(VTTScanner& input, double& time_stamp) {
  // Collect a WebVTT timestamp (5.3 WebVTT cue timings and settings parsing.)
  // Steps 1 - 4 - Initial checks, let most significant units be minutes.
  enum Mode { kMinutes, kHours };
  Mode mode = kMinutes;

  // Steps 5 - 7 - Collect a sequence of characters that are 0-9.
  // If not 2 characters or value is greater than 59, interpret as hours.
  unsigned value1;
  const size_t value1_digits = input.ScanDigits(value1);
  if (!value1_digits)
    return false;
  if (value1_digits != 2 || value1 > 59)
    mode = kHours;

  // Steps 8 - 11 - Collect the next sequence of 0-9 after ':' (must be 2
  // chars).
  unsigned value2;
  if (!input.Scan(':') || input.ScanDigits(value2) != 2)
    return false;

  // Step 12 - Detect whether this timestamp includes hours.
  unsigned value3;
  if (mode == kHours || input.Match(':')) {
    if (!input.Scan(':') || input.ScanDigits(value3) != 2)
      return false;
  } else {
    value3 = value2;
    value2 = value1;
    value1 = 0;
  }

  // Steps 13 - 17 - Collect next sequence of 0-9 after '.' (must be 3 chars).
  unsigned value4;
  if (!input.Scan('.') || input.ScanDigits(value4) != 3)
    return false;
  if (value2 > 59 || value3 > 59)
    return false;

  // Steps 18 - 19 - Calculate result.
  time_stamp = (value1 * kMinutesPerHour * kSecondsPerMinute) +
               (value2 * kSecondsPerMinute) + value3 +
               (value4 * (1 / kMsPerSecond));
  return true;
}

static VttNodeType TokenToNodeType(VTTToken& token) {
  switch (token.GetName().length()) {
    case 1:
      if (token.GetName()[0] == 'c')
        return VttNodeType::kClass;
      if (token.GetName()[0] == 'v')
        return VttNodeType::kVoice;
      if (token.GetName()[0] == 'b')
        return VttNodeType::kBold;
      if (token.GetName()[0] == 'i')
        return VttNodeType::kItalic;
      if (token.GetName()[0] == 'u')
        return VttNodeType::kUnderline;
      break;
    case 2:
      if (token.GetName()[0] == 'r' && token.GetName()[1] == 't')
        return VttNodeType::kRubyText;
      break;
    case 4:
      if (token.GetName()[0] == 'r' && token.GetName()[1] == 'u' &&
          token.GetName()[2] == 'b' && token.GetName()[3] == 'y')
        return VttNodeType::kRuby;
      if (token.GetName()[0] == 'l' && token.GetName()[1] == 'a' &&
          token.GetName()[2] == 'n' && token.GetName()[3] == 'g')
        return VttNodeType::kLanguage;
      break;
  }
  return VttNodeType::kNone;
}

void VTTTreeBuilder::ConstructTreeFromToken(Document& document) {
  // http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules

  switch (token_.GetType()) {
    case VTTTokenTypes::kCharacter: {
      current_node_->ParserAppendChild(
          Text::Create(document, token_.Characters()));
      break;
    }
    case VTTTokenTypes::kStartTag: {
      VttNodeType node_type = TokenToNodeType(token_);
      if (node_type == VttNodeType::kNone) {
        break;
      }

      auto* curr_vtt_element = DynamicTo<VTTElement>(current_node_);
      VttNodeType current_type = curr_vtt_element
                                     ? curr_vtt_element->GetVttNodeType()
                                     : VttNodeType::kNone;
      // <rt> is only allowed if the current node is <ruby>.
      if (node_type == VttNodeType::kRubyText &&
          current_type != VttNodeType::kRuby) {
        break;
      }

      auto* child = MakeGarbageCollected<VTTElement>(node_type, &document);
      child->SetTrack(track_);

      if (!token_.Classes().empty())
        child->setAttribute(html_names::kClassAttr, token_.Classes());

      if (node_type == VttNodeType::kVoice) {
        child->setAttribute(VTTElement::VoiceAttributeName(),
                            token_.Annotation());
      } else if (node_type == VttNodeType::kLanguage) {
        language_stack_.push_back(token_.Annotation());
        child->setAttribute(VTTElement::LangAttributeName(),
                            language_stack_.back());
      }
      if (!language_stack_.empty())
        child->SetLanguage(language_stack_.back());
      current_node_->ParserAppendChild(child);
      current_node_ = child;
      break;
    }
    case VTTTokenTypes::kEndTag: {
      VttNodeType node_type = TokenToNodeType(token_);
      if (node_type == VttNodeType::kNone) {
        break;
      }

      // The only non-VTTElement would be the DocumentFragment root. (Text
      // nodes and PIs will never appear as current_node_.)
      auto* curr_vtt_element = DynamicTo<VTTElement>(current_node_);
      if (!curr_vtt_element)
        break;

      VttNodeType current_type = curr_vtt_element->GetVttNodeType();
      bool matches_current = node_type == current_type;
      if (!matches_current) {
        // </ruby> auto-closes <rt>.
        if (current_type == VttNodeType::kRubyText &&
            node_type == VttNodeType::kRuby) {
          if (current_node_->parentNode())
            current_node_ = current_node_->parentNode();
        } else {
          break;
        }
      }
      if (node_type == VttNodeType::kLanguage) {
        language_stack_.pop_back();
      }
      if (current_node_->parentNode())
        current_node_ = current_node_->parentNode();
      break;
    }
    case VTTTokenTypes::kTimestampTag: {
      double parsed_time_stamp;
      if (VTTParser::CollectTimeStamp(token_.Characters(), parsed_time_stamp)) {
        current_node_->ParserAppendChild(
            MakeGarbageCollected<ProcessingInstruction>(
                document, "timestamp", SerializeTimeStamp(parsed_time_stamp)));
      }
      break;
    }
    default:
      break;
  }
}

void VTTParser::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(current_region_);
  visitor->Trace(client_);
  visitor->Trace(cue_list_);
  visitor->Trace(region_map_);
  visitor->Trace(style_sheets_);
}

}  // namespace blink
