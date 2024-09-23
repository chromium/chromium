// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document_statistics_collector.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_distillability.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

// Saturate the length of a paragraph to save time.
const int kTextContentLengthSaturation = 1000;

// Filter out short P elements. The threshold is set to around 2 English
// sentences.
const unsigned kParagraphLengthThreshold = 140;

// Saturate the scores to save time. The max is the score of 6 long paragraphs.
// 6 * sqrt(kTextContentLengthSaturation - kParagraphLengthThreshold)
const double kMozScoreSaturation = 175.954539583;
// 6 * sqrt(kTextContentLengthSaturation);
const double kMozScoreAllSqrtSaturation = 189.73665961;
const double kMozScoreAllLinearSaturation = 6 * kTextContentLengthSaturation;

unsigned TextContentLengthSaturated(const Element& root) {
  unsigned length = 0;
  // This skips shadow DOM intentionally, to match the JavaScript
  // implementation.  We would like to use the same statistics extracted by the
  // JavaScript implementation on iOS, and JavaScript cannot peek deeply into
  // shadow DOM except on modern Chrome versions.
  // Given shadow DOM rarely appears in <P> elements in long-form articles, the
  // overall accuracy should not be largely affected.
  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    auto* text_node = DynamicTo<Text>(node);
    if (!text_node) {
      continue;
    }
    length += text_node->length();
    if (length > kTextContentLengthSaturation) {
      return kTextContentLengthSaturation;
    }
  }
  return length;
}

bool IsVisible(const Element& element) {
  const ComputedStyle* style = element.GetComputedStyle();
  if (!style)
    return false;
  return (style->Display() != EDisplay::kNone &&
          style->UsedVisibility() != EVisibility::kHidden &&
          style->Opacity() != 0);
}

bool MatchAttributes(const Element& element, const Vector<String>& words) {
  const String& classes = element.GetClassAttribute();
  const String& id = element.GetIdAttribute();
  for (const String& word : words) {
    if (classes.FindIgnoringCase(word) != WTF::kNotFound ||
        id.FindIgnoringCase(word) != WTF::kNotFound) {
      return true;
    }
  }
  return false;
}

bool IsGoodForScoring(const WebDistillabilityFeatures& features,
                      const Element& element) {
  DEFINE_STATIC_LOCAL(Vector<String>, unlikely_candidates, ());
  if (unlikely_candidates.empty()) {
    auto words = {
        "banner",  "combx",      "comment", "community",  "disqus",  "extra",
        "foot",    "header",     "menu",    "related",    "remark",  "rss",
        "share",   "shoutbox",   "sidebar", "skyscraper", "sponsor", "ad-break",
        "agegate", "pagination", "pager",   "popup"};
    for (auto* word : words) {
      unlikely_candidates.push_back(word);
    }
  }
  DEFINE_STATIC_LOCAL(Vector<String>, highly_likely_candidates, ());
  if (highly_likely_candidates.empty()) {
    auto words = {"and", "article", "body", "column", "main", "shadow"};
    for (auto* word : words) {
      highly_likely_candidates.push_back(word);
    }
  }

  if (!IsVisible(element))
    return false;
  if (features.moz_score >= kMozScoreSaturation &&
      features.moz_score_all_sqrt >= kMozScoreAllSqrtSaturation &&
      features.moz_score_all_linear >= kMozScoreAllLinearSaturation)
    return false;
  if (MatchAttributes(element, unlikely_candidates) &&
      !MatchAttributes(element, highly_likely_candidates))
    return false;
  return true;
}

// underListItem denotes that at least one of the ancesters is <li> element.
void CollectFeatures(Element& root,
                     WebDistillabilityFeatures& features,
                     bool under_list_item = false) {
  for (Element& element : ElementTraversal::ChildrenOf(root)) {
    bool is_list_item = false;
    features.element_count++;
    if (element.HasTagName(html_names::kATag)) {
      features.anchor_count++;
    } else if (element.HasTagName(html_names::kFormTag)) {
      features.form_count++;
    } else if (element.HasTagName(html_names::kInputTag)) {
      const auto& input = To<HTMLInputElement>(element);
      if (input.FormControlType() == FormControlType::kInputText) {
        features.text_input_count++;
      } else if (input.FormControlType() == FormControlType::kInputPassword) {
        features.password_input_count++;
      }
    } else if (element.HasTagName(html_names::kPTag) ||
               element.HasTagName(html_names::kPreTag)) {
      if (element.HasTagName(html_names::kPTag)) {
        features.p_count++;
      } else {
        features.pre_count++;
      }
      if (!under_list_item && IsGoodForScoring(features, element)) {
        unsigned length = TextContentLengthSaturated(element);
        if (length >= kParagraphLengthThreshold) {
          features.moz_score += sqrt(length - kParagraphLengthThreshold);
          features.moz_score =
              std::min(features.moz_score, kMozScoreSaturation);
        }
        features.moz_score_all_sqrt += sqrt(length);
        features.moz_score_all_sqrt =
            std::min(features.moz_score_all_sqrt, kMozScoreAllSqrtSaturation);

        features.moz_score_all_linear += length;
        features.moz_score_all_linear = std::min(features.moz_score_all_linear,
                                                 kMozScoreAllLinearSaturation);
      }
    } else if (element.HasTagName(html_names::kLiTag)) {
      is_list_item = true;
    }
    CollectFeatures(element, features, under_list_item || is_list_item);
  }
}

bool HasOpenGraphArticle(const Element& head) {
  DEFINE_STATIC_LOCAL(AtomicString, og_type, ("og:type"));
  DEFINE_STATIC_LOCAL(AtomicString, property_attr, ("property"));
  for (const Element* child = ElementTraversal::FirstChild(head); child;
       child = ElementTraversal::NextSibling(*child)) {
    auto* meta = DynamicTo<HTMLMetaElement>(child);
    if (!meta)
      continue;

    if (meta->GetName() == og_type ||
        meta->getAttribute(property_attr) == og_type) {
      if (EqualIgnoringASCIICase(meta->Content(), "article")) {
        return true;
      }
    }
  }
  return false;
}

bool IsMobileFriendly(Document& document) {
  if (Page* page = document.GetPage())
    return page->GetVisualViewport().ShouldDisableDesktopWorkarounds();
  return false;
}

}  // namespace

WebDistillabilityFeatures DocumentStatisticsCollector::CollectStatistics(
    Document& document) {
  TRACE_EVENT0("blink", "DocumentStatisticsCollector::collectStatistics");

  WebDistillabilityFeatures features = WebDistillabilityFeatures();

  if (!document.GetFrame() || !document.GetFrame()->IsOutermostMainFrame())
    return features;

  DCHECK(document.HasFinishedParsing());

  HTMLElement* body = document.body();
  HTMLElement* head = document.head();

  if (!body || !head)
    return features;

  features.is_mobile_friendly = IsMobileFriendly(document);

  base::TimeTicks start_time = base::TimeTicks::Now();

  // This should be cheap since collectStatistics is only called right after
  // layout.
  document.UpdateStyleAndLayoutTree();

  // Traverse the DOM tree and collect statistics.
  CollectFeatures(*body, features);
  features.open_graph = HasOpenGraphArticle(*head);

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  DEFINE_STATIC_LOCAL(CustomCountHistogram, distillability_histogram,
                      ("WebCore.DistillabilityUs", 1, 1000000, 50));
  distillability_histogram.CountMicroseconds(elapsed_time);

  return features;
}

}  // namespace blink
