/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"

#include <algorithm>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

static bool CompareByDensity(const ImageCandidate& first,
                             const ImageCandidate& second) {
  return first.Density() < second.Density();
}

enum DescriptorTokenizerState {
  kTokenStart,
  kInParenthesis,
  kAfterToken,
};

struct DescriptorToken {
  unsigned start;
  unsigned length;

  DescriptorToken(unsigned start, unsigned length)
      : start(start), length(length) {}

  unsigned LastIndex() { return start + length - 1; }

  template <typename CharType>
  int ToInt(const CharType* attribute, bool& is_valid) {
    unsigned position = 0;
    // Make sure the integer is a valid non-negative integer
    // https://html.spec.whatwg.org/C/#valid-non-negative-integer
    unsigned length_excluding_descriptor = length - 1;
    while (position < length_excluding_descriptor) {
      if (!IsASCIIDigit(*(attribute + start + position))) {
        is_valid = false;
        return 0;
      }
      ++position;
    }
    return CharactersToInt(base::span<const CharType>(
                               attribute + start, length_excluding_descriptor),
                           WTF::NumberParsingOptions(), &is_valid);
  }

  template <typename CharType>
  float ToFloat(const CharType* attribute, bool& is_valid) {
    // Make sure the is a valid floating point number
    // https://html.spec.whatwg.org/C/#valid-floating-point-number
    unsigned length_excluding_descriptor = length - 1;
    if (length_excluding_descriptor > 0 && *(attribute + start) == '+') {
      is_valid = false;
      return 0;
    }
    Decimal result = ParseToDecimalForNumberType(
        String(attribute + start, length_excluding_descriptor));
    is_valid = result.IsFinite();
    if (!is_valid)
      return 0;
    return static_cast<float>(result.ToDouble());
  }
};

template <typename CharType>
static void AppendDescriptorAndReset(const CharType* attribute_start,
                                     const CharType*& descriptor_start,
                                     const CharType* position,
                                     Vector<DescriptorToken>& descriptors) {
  if (position > descriptor_start) {
    descriptors.push_back(DescriptorToken(
        static_cast<unsigned>(descriptor_start - attribute_start),
        static_cast<unsigned>(position - descriptor_start)));
  }
  descriptor_start = nullptr;
}

// The following is called appendCharacter to match the spec's terminology.
template <typename CharType>
static void AppendCharacter(const CharType* descriptor_start,
                            const CharType* position) {
  // Since we don't copy the tokens, this just set the point where the
  // descriptor tokens start.
  if (!descriptor_start)
    descriptor_start = position;
}

template <typename CharType>
static bool IsEOF(const CharType* position, const CharType* end) {
  return position >= end;
}

template <typename CharType>
static void TokenizeDescriptors(const CharType* attribute_start,
                                const CharType*& position,
                                const CharType* attribute_end,
                                Vector<DescriptorToken>& descriptors) {
  DescriptorTokenizerState state = kTokenStart;
  const CharType* descriptors_start = position;
  const CharType* current_descriptor_start = descriptors_start;
  while (true) {
    switch (state) {
      case kTokenStart:
        if (IsEOF(position, attribute_end)) {
          AppendDescriptorAndReset(attribute_start, current_descriptor_start,
                                   attribute_end, descriptors);
          return;
        }
        if (IsComma(*position)) {
          AppendDescriptorAndReset(attribute_start, current_descriptor_start,
                                   position, descriptors);
          ++position;
          return;
        }
        if (IsHTMLSpace(*position)) {
          AppendDescriptorAndReset(attribute_start, current_descriptor_start,
                                   position, descriptors);
          current_descriptor_start = position + 1;
          state = kAfterToken;
        } else if (*position == '(') {
          AppendCharacter(current_descriptor_start, position);
          state = kInParenthesis;
        } else {
          AppendCharacter(current_descriptor_start, position);
        }
        break;
      case kInParenthesis:
        if (IsEOF(position, attribute_end)) {
          AppendDescriptorAndReset(attribute_start, current_descriptor_start,
                                   attribute_end, descriptors);
          return;
        }
        if (*position == ')') {
          AppendCharacter(current_descriptor_start, position);
          state = kTokenStart;
        } else {
          AppendCharacter(current_descriptor_start, position);
        }
        break;
      case kAfterToken:
        if (IsEOF(position, attribute_end))
          return;
        if (!IsHTMLSpace(*position)) {
          state = kTokenStart;
          current_descriptor_start = position;
          --position;
        }
        break;
    }
    ++position;
  }
}

static void SrcsetError(Document* document, String message) {
  if (document && document->GetFrame()) {
    StringBuilder warning_message;
    warning_message.Append("Failed parsing 'srcset' attribute value since ");
    warning_message.Append(message);
    document->GetFrame()->Console().AddMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kOther,
            mojom::ConsoleMessageLevel::kWarning, warning_message.ToString()));
  }
}

template <typename CharType>
static bool ParseDescriptors(const CharType* attribute,
                             Vector<DescriptorToken>& descriptors,
                             DescriptorParsingResult& result,
                             Document* document) {
  for (DescriptorToken& descriptor : descriptors) {
    if (descriptor.length == 0)
      continue;
    CharType c = attribute[descriptor.LastIndex()];
    bool is_valid = false;
    if (c == 'w') {
      if (result.HasDensity() || result.HasWidth()) {
        SrcsetError(document,
                    "it has multiple 'w' descriptors or a mix of 'x' and 'w' "
                    "descriptors.");
        return false;
      }
      int resource_width = descriptor.ToInt(attribute, is_valid);
      if (!is_valid || resource_width <= 0) {
        SrcsetError(document, "its 'w' descriptor is invalid.");
        return false;
      }
      result.SetResourceWidth(resource_width);
    } else if (c == 'h') {
      // This is here only for future compat purposes. The value of the 'h'
      // descriptor is not used.
      if (result.HasDensity() || result.HasHeight()) {
        SrcsetError(document,
                    "it has multiple 'h' descriptors or a mix of 'x' and 'h' "
                    "descriptors.");
        return false;
      }
      int resource_height = descriptor.ToInt(attribute, is_valid);
      if (!is_valid || resource_height <= 0) {
        SrcsetError(document, "its 'h' descriptor is invalid.");
        return false;
      }
      result.SetResourceHeight(resource_height);
    } else if (c == 'x') {
      if (result.HasDensity() || result.HasHeight() || result.HasWidth()) {
        SrcsetError(document,
                    "it has multiple 'x' descriptors or a mix of 'x' and "
                    "'w'/'h' descriptors.");
        return false;
      }
      float density = descriptor.ToFloat(attribute, is_valid);
      if (!is_valid || density < 0) {
        SrcsetError(document, "its 'x' descriptor is invalid.");
        return false;
      }
      result.SetDensity(density);
    } else {
      SrcsetError(document, "it has an unknown descriptor.");
      return false;
    }
  }
  bool res = !result.HasHeight() || result.HasWidth();
  if (!res)
    SrcsetError(document, "it has an 'h' descriptor and no 'w' descriptor.");
  return res;
}

static bool ParseDescriptors(const String& attribute,
                             Vector<DescriptorToken>& descriptors,
                             DescriptorParsingResult& result,
                             Document* document) {
  // FIXME: See if StringView can't be extended to replace DescriptorToken here.
  return WTF::VisitCharacters(attribute, [&](auto chars) {
    return ParseDescriptors(chars.data(), descriptors, result, document);
  });
}

// http://picture.responsiveimages.org/#parse-srcset-attr
template <typename CharType>
static void ParseImageCandidatesFromSrcsetAttribute(
    const String& attribute,
    const CharType* attribute_start,
    unsigned length,
    Vector<ImageCandidate>& image_candidates,
    Document* document) {
  const CharType* position = attribute_start;
  const CharType* attribute_end = position + length;

  while (position < attribute_end) {
    // 4. Splitting loop: Collect a sequence of characters that are space
    // characters or U+002C COMMA characters.
    SkipWhile<CharType, IsHTMLSpaceOrComma<CharType>>(position, attribute_end);
    if (position == attribute_end) {
      // Contrary to spec language - descriptor parsing happens on each
      // candidate, so when we reach the attributeEnd, we can exit.
      break;
    }
    const CharType* image_url_start = position;

    // 6. Collect a sequence of characters that are not space characters, and
    // let that be url.
    SkipUntil<CharType, IsHTMLSpace<CharType>>(position, attribute_end);
    const CharType* image_url_end = position;

    DescriptorParsingResult result;

    // 8. If url ends with a U+002C COMMA character (,)
    if (IsComma(*(position - 1))) {
      // Remove all trailing U+002C COMMA characters from url.
      image_url_end = position - 1;
      ReverseSkipWhile<CharType, IsComma>(image_url_end, image_url_start);
      ++image_url_end;
      // If url is empty, then jump to the step labeled splitting loop.
      if (image_url_start == image_url_end)
        continue;
    } else {
      SkipWhile<CharType, IsHTMLSpace<CharType>>(position, attribute_end);
      Vector<DescriptorToken> descriptor_tokens;
      TokenizeDescriptors(attribute_start, position, attribute_end,
                          descriptor_tokens);
      // Contrary to spec language - descriptor parsing happens on each
      // candidate. This is a black-box equivalent, to avoid storing descriptor
      // lists for each candidate.
      if (!ParseDescriptors(attribute, descriptor_tokens, result, document)) {
        if (document) {
          UseCounter::Count(document, WebFeature::kSrcsetDroppedCandidate);
          if (document->GetFrame()) {
            document->GetFrame()->Console().AddMessage(
                MakeGarbageCollected<ConsoleMessage>(
                    mojom::ConsoleMessageSource::kOther,
                    mojom::ConsoleMessageLevel::kWarning,
                    String("Dropped srcset candidate ") +
                        JSONValue::QuoteString(
                            String(image_url_start,
                                   static_cast<wtf_size_t>(image_url_end -
                                                           image_url_start)))));
          }
        }
        continue;
      }
    }

    DCHECK_GT(image_url_end, attribute_start);
    unsigned image_url_starting_position =
        static_cast<unsigned>(image_url_start - attribute_start);
    DCHECK_GT(image_url_end, image_url_start);
    unsigned image_url_length =
        static_cast<unsigned>(image_url_end - image_url_start);
    image_candidates.push_back(
        ImageCandidate(attribute, image_url_starting_position, image_url_length,
                       result, ImageCandidate::kSrcsetOrigin));
    // 11. Return to the step labeled splitting loop.
  }
}

static void ParseImageCandidatesFromSrcsetAttribute(
    const String& attribute,
    Vector<ImageCandidate>& image_candidates,
    Document* document) {
  if (attribute.IsNull())
    return;

  if (attribute.Is8Bit())
    ParseImageCandidatesFromSrcsetAttribute<LChar>(
        attribute, attribute.Characters8(), attribute.length(),
        image_candidates, document);
  else
    ParseImageCandidatesFromSrcsetAttribute<UChar>(
        attribute, attribute.Characters16(), attribute.length(),
        image_candidates, document);
}

static unsigned SelectionLogic(Vector<ImageCandidate*>& image_candidates,
                               float device_scale_factor) {
  unsigned i = 0;

  for (; i < image_candidates.size() - 1; ++i) {
    unsigned next = i + 1;
    float next_density;
    float current_density;
    float geometric_mean;

    next_density = image_candidates[next]->Density();
    if (next_density < device_scale_factor)
      continue;

    current_density = image_candidates[i]->Density();
    geometric_mean = sqrt(current_density * next_density);
    if (((device_scale_factor <= 1.0) &&
         (device_scale_factor > current_density)) ||
        (device_scale_factor >= geometric_mean))
      return next;
    break;
  }
  return i;
}

static unsigned AvoidDownloadIfHigherDensityResourceIsInCache(
    Vector<ImageCandidate*>& image_candidates,
    unsigned winner,
    Document* document) {
  if (!document)
    return winner;
  for (unsigned i = image_candidates.size() - 1; i > winner; --i) {
    KURL url = document->CompleteURL(
        StripLeadingAndTrailingHTMLSpaces(image_candidates[i]->Url()));
    auto* resource = MemoryCache::Get()->ResourceForURL(
        url, document->Fetcher()->GetCacheIdentifier(url));
    if ((resource && resource->IsLoaded()) || url.ProtocolIsData()) {
      return i;
    }
  }
  return winner;
}

static ImageCandidate PickBestImageCandidate(
    float device_scale_factor,
    float source_size,
    Vector<ImageCandidate>& image_candidates,
    Document* document = nullptr) {
  const float kDefaultDensityValue = 1.0;
  // The srcset image source selection mechanism is user-agent specific:
  // https://html.spec.whatwg.org/multipage/images.html#selecting-an-image-source
  //
  // Setting max density value based on https://github.com/whatwg/html/pull/5901
  const float kMaxDensity = 2.2;
  bool ignore_src = false;
  if (image_candidates.empty())
    return ImageCandidate();

  if (RuntimeEnabledFeatures::SrcsetMaxDensityEnabled() &&
      device_scale_factor > kMaxDensity) {
    device_scale_factor = kMaxDensity;
  }

  // http://picture.responsiveimages.org/#normalize-source-densities
  for (ImageCandidate& image : image_candidates) {
    if (image.GetResourceWidth() > 0) {
      image.SetDensity((float)image.GetResourceWidth() / source_size);
      ignore_src = true;
    } else if (image.Density() < 0) {
      image.SetDensity(kDefaultDensityValue);
    }
  }

  std::stable_sort(image_candidates.begin(), image_candidates.end(),
                   CompareByDensity);

  Vector<ImageCandidate*> de_duped_image_candidates;
  float prev_density = -1.0;
  for (ImageCandidate& image : image_candidates) {
    if (image.Density() != prev_density && (!ignore_src || !image.SrcOrigin()))
      de_duped_image_candidates.push_back(&image);
    prev_density = image.Density();
  }

  unsigned winner =
      blink::WebNetworkStateNotifier::SaveDataEnabled() &&
              base::FeatureList::IsEnabled(blink::features::kSaveDataImgSrcset)
          ? 0
          : SelectionLogic(de_duped_image_candidates, device_scale_factor);
  DCHECK_LT(winner, de_duped_image_candidates.size());
  winner = AvoidDownloadIfHigherDensityResourceIsInCache(
      de_duped_image_candidates, winner, document);

  float winning_density = de_duped_image_candidates[winner]->Density();
  // 16. If an entry b in candidates has the same associated ... pixel density
  // as an earlier entry a in candidates,
  // then remove entry b
  while ((winner > 0) &&
         (de_duped_image_candidates[winner - 1]->Density() == winning_density))
    --winner;

  return *de_duped_image_candidates[winner];
}

ImageCandidate BestFitSourceForSrcsetAttribute(float device_scale_factor,
                                               float source_size,
                                               const String& srcset_attribute,
                                               Document* document) {
  Vector<ImageCandidate> image_candidates;

  ParseImageCandidatesFromSrcsetAttribute(srcset_attribute, image_candidates,
                                          document);

  return PickBestImageCandidate(device_scale_factor, source_size,
                                image_candidates, document);
}

ImageCandidate BestFitSourceForImageAttributes(float device_scale_factor,
                                               float source_size,
                                               const String& src_attribute,
                                               const String& srcset_attribute,
                                               Document* document) {
  if (srcset_attribute.IsNull()) {
    if (src_attribute.IsNull())
      return ImageCandidate();
    return ImageCandidate(src_attribute, 0, src_attribute.length(),
                          DescriptorParsingResult(),
                          ImageCandidate::kSrcOrigin);
  }

  Vector<ImageCandidate> image_candidates;

  ParseImageCandidatesFromSrcsetAttribute(srcset_attribute, image_candidates,
                                          document);

  if (!src_attribute.empty())
    image_candidates.push_back(
        ImageCandidate(src_attribute, 0, src_attribute.length(),
                       DescriptorParsingResult(), ImageCandidate::kSrcOrigin));

  return PickBestImageCandidate(device_scale_factor, source_size,
                                image_candidates, document);
}

String BestFitSourceForImageAttributes(float device_scale_factor,
                                       float source_size,
                                       const String& src_attribute,
                                       ImageCandidate& srcset_image_candidate) {
  if (srcset_image_candidate.IsEmpty())
    return src_attribute;

  Vector<ImageCandidate> image_candidates;
  image_candidates.push_back(srcset_image_candidate);

  if (!src_attribute.empty())
    image_candidates.push_back(
        ImageCandidate(src_attribute, 0, src_attribute.length(),
                       DescriptorParsingResult(), ImageCandidate::kSrcOrigin));

  return PickBestImageCandidate(device_scale_factor, source_size,
                                image_candidates)
      .ToString();
}

}  // namespace blink
