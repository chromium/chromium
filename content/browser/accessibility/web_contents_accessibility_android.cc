// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "content/browser/accessibility/web_contents_accessibility_android.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/fixed_array.h"
#include "content/browser/accessibility/accessibility_tree_snapshot_combiner.h"
#include "content/browser/accessibility/ax_style_data.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/accessibility/browser_accessibility_state_impl_android.h"
#include "content/browser/accessibility/text_formatting_metrics_android.h"
#include "content/browser/android/render_widget_host_connector.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/common/content_features.h"
#include "net/base/data_url.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_prefs.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/accessibility/platform/one_shot_accessibility_tree_search.h"
#include "ui/events/android/motion_event_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/AccessibilityNodeInfoBuilder_jni.h"
#include "content/public/android/content_jni_headers/AccessibilityNodeInfoUtils_jni.h"
#include "content/public/android/content_jni_headers/AssistDataBuilder_jni.h"
#include "content/public/android/content_jni_headers/WebContentsAccessibilityImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;

namespace content {

namespace {

using RangePairs = AXStyleData::RangePairs;

// This map contains key value pairs of a string to a tree search predicate. The
// set of keys represents the ways in which an AT can navigate a page by HTML
// element (by next or previous navigation). A Java-side AT sends a key with a
// next/previous action request, and this map is used to map the string to the
// correct predicate.
using SearchKeyToPredicateMap =
    absl::flat_hash_map<std::u16string, ui::AccessibilityMatchPredicate>;

static const char kHtmlTypeRow[] = "ROW";
static const char kHtmlTypeColumn[] = "COLUMN";
static const char kHtmlTypeRowBounds[] = "ROW_BOUNDS";
static const char kHtmlTypeColumnBounds[] = "COLUMN_BOUNDS";
static const char kHtmlTypeTableBounds[] = "TABLE_BOUNDS";

// IMPORTANT!
// These values are written to logs. Do not renumber or delete
// existing items; add new entries to the end of the list.
//
// LINT.IfChange
enum class AccessibilityPredicateType {
  // Used for a string we do not support/recognize.
  kUnknown = 0,

  // Used for an empty string, which is default and maps to
  // "IsInterestingOnAndroid".
  kDefault = 1,

  // Currently supported navigation types.
  kArticle = 2,
  kBlockQuote = 3,
  kButton = 4,
  kCheckbox = 5,
  kCombobox = 6,
  kControl = 7,
  kFocusable = 8,
  kFrame = 9,
  kGraphic = 10,
  kH1 = 11,
  kH2 = 12,
  kH3 = 13,
  kH4 = 14,
  kH5 = 15,
  kH6 = 16,
  kHeading = 17,
  kHeadingSame = 18,
  kLandmark = 19,
  kLink = 20,
  kList = 21,
  kListItem = 22,
  kLive = 23,
  kMain = 24,
  kMedia = 25,
  kParagraph = 26,
  kRadio = 27,
  kRadioGroup = 28,
  kSection = 29,
  kTable = 30,
  kTextfield = 31,
  kTextBold = 32,
  kTextItalic = 33,
  kTextUnderline = 34,
  kTree = 35,
  kUnvisitedLink = 36,
  kVisitedLink = 37,
  kRow = 38,
  kColumn = 39,
  kRowBounds = 40,
  kColumnBounds = 41,
  kTableBounds = 42,

  // Max value, must always be equal to the largest entry logged, remember to
  // increment.
  kMaxValue = 42
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityPredicateType)

// This map contains key value pairs of a string to an internal enum identifying
// a tree search predicate. A Java-side AT sends a key with a next/previous
// action request, and this map is used to map the string to an enum so we can
// log a histogram.
using PredicateToEnumMap =
    absl::flat_hash_map<std::u16string, AccessibilityPredicateType>;

bool AllInterestingNodesPredicate(ui::BrowserAccessibility* start,
                                  ui::BrowserAccessibility* node) {
  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);
  return android_node->IsInterestingOnAndroid();
}

bool AccessibilityNoOpPredicate(ui::BrowserAccessibility* start,
                                ui::BrowserAccessibility* node) {
  return true;
}

using SearchKeyData =
    std::tuple<SearchKeyToPredicateMap, PredicateToEnumMap, std::u16string>;

// Getter function for the search key to predicate map and all keys string.
const SearchKeyData& GetSearchKeyData() {
  static base::NoDestructor<SearchKeyData> search_key_data([] {
    // These are special unofficial strings sent from TalkBack/BrailleBack
    // to jump to certain categories of web elements.
    SearchKeyToPredicateMap search_key_to_predicate_map;
    PredicateToEnumMap predicate_to_enum_map;
    std::vector<std::u16string> all_search_keys;

    auto add_to_map = [&](std::u16string key,
                          ui::AccessibilityMatchPredicate predicate,
                          AccessibilityPredicateType type) {
      search_key_to_predicate_map[key] = std::move(predicate);
      predicate_to_enum_map[key] = type;
      all_search_keys.emplace_back(std::move(key));
    };

    add_to_map(u"ARTICLE", ui::AccessibilityArticlePredicate,
               AccessibilityPredicateType::kArticle);
    add_to_map(u"BLOCKQUOTE", ui::AccessibilityBlockquotePredicate,
               AccessibilityPredicateType::kBlockQuote);
    add_to_map(u"BUTTON", ui::AccessibilityButtonPredicate,
               AccessibilityPredicateType::kButton);
    add_to_map(u"CHECKBOX", ui::AccessibilityCheckboxPredicate,
               AccessibilityPredicateType::kCheckbox);
    add_to_map(u"COMBOBOX", ui::AccessibilityComboboxPredicate,
               AccessibilityPredicateType::kCombobox);
    add_to_map(u"CONTROL", ui::AccessibilityControlPredicate,
               AccessibilityPredicateType::kControl);
    add_to_map(u"FOCUSABLE", ui::AccessibilityFocusablePredicate,
               AccessibilityPredicateType::kFocusable);
    add_to_map(u"FRAME", ui::AccessibilityFramePredicate,
               AccessibilityPredicateType::kFrame);
    add_to_map(u"GRAPHIC", ui::AccessibilityGraphicPredicate,
               AccessibilityPredicateType::kGraphic);
    add_to_map(u"H1", ui::AccessibilityH1Predicate,
               AccessibilityPredicateType::kH1);
    add_to_map(u"H2", ui::AccessibilityH2Predicate,
               AccessibilityPredicateType::kH2);
    add_to_map(u"H3", ui::AccessibilityH3Predicate,
               AccessibilityPredicateType::kH3);
    add_to_map(u"H4", ui::AccessibilityH4Predicate,
               AccessibilityPredicateType::kH4);
    add_to_map(u"H5", ui::AccessibilityH5Predicate,
               AccessibilityPredicateType::kH5);
    add_to_map(u"H6", ui::AccessibilityH6Predicate,
               AccessibilityPredicateType::kH6);
    add_to_map(u"HEADING", ui::AccessibilityHeadingPredicate,
               AccessibilityPredicateType::kHeading);
    add_to_map(u"HEADING_SAME", ui::AccessibilityHeadingSameLevelPredicate,
               AccessibilityPredicateType::kHeadingSame);
    add_to_map(u"LANDMARK", ui::AccessibilityLandmarkPredicate,
               AccessibilityPredicateType::kLandmark);
    add_to_map(u"LINK", ui::AccessibilityLinkPredicate,
               AccessibilityPredicateType::kLink);
    add_to_map(u"LIST", ui::AccessibilityListPredicate,
               AccessibilityPredicateType::kList);
    add_to_map(u"LIST_ITEM", ui::AccessibilityListItemPredicate,
               AccessibilityPredicateType::kListItem);
    add_to_map(u"LIVE", ui::AccessibilityLiveRegionPredicate,
               AccessibilityPredicateType::kLive);
    add_to_map(u"MAIN", ui::AccessibilityMainPredicate,
               AccessibilityPredicateType::kMain);
    add_to_map(u"MEDIA", ui::AccessibilityMediaPredicate,
               AccessibilityPredicateType::kMedia);
    add_to_map(u"PARAGRAPH", ui::AccessibilityParagraphPredicate,
               AccessibilityPredicateType::kParagraph);
    add_to_map(u"RADIO", ui::AccessibilityRadioButtonPredicate,
               AccessibilityPredicateType::kRadio);
    add_to_map(u"RADIO_GROUP", ui::AccessibilityRadioGroupPredicate,
               AccessibilityPredicateType::kRadioGroup);
    add_to_map(u"SECTION", ui::AccessibilitySectionPredicate,
               AccessibilityPredicateType::kSection);
    add_to_map(u"TABLE", ui::AccessibilityTablePredicate,
               AccessibilityPredicateType::kTable);
    add_to_map(u"TEXT_FIELD", ui::AccessibilityTextfieldPredicate,
               AccessibilityPredicateType::kTextfield);
    add_to_map(u"TEXT_BOLD", ui::AccessibilityTextStyleBoldPredicate,
               AccessibilityPredicateType::kTextBold);
    add_to_map(u"TEXT_ITALIC", ui::AccessibilityTextStyleItalicPredicate,
               AccessibilityPredicateType::kTextItalic);
    add_to_map(u"TEXT_UNDER", ui::AccessibilityTextStyleUnderlinePredicate,
               AccessibilityPredicateType::kTextUnderline);
    add_to_map(u"TREE", ui::AccessibilityTreePredicate,
               AccessibilityPredicateType::kTree);
    add_to_map(u"UNVISITED_LINK", ui::AccessibilityUnvisitedLinkPredicate,
               AccessibilityPredicateType::kUnvisitedLink);
    add_to_map(u"VISITED_LINK", ui::AccessibilityVisitedLinkPredicate,
               AccessibilityPredicateType::kVisitedLink);

    // These are surfaced simply to document the html types, but do not do a
    // tree/predicate search.
    add_to_map(u"ROW", AccessibilityNoOpPredicate,
               AccessibilityPredicateType::kRow);
    add_to_map(u"ROW", AccessibilityNoOpPredicate,
               AccessibilityPredicateType::kRow);
    add_to_map(u"COLUMN", AccessibilityNoOpPredicate,
               AccessibilityPredicateType::kColumn);
    add_to_map(u"ROW_BOUNDS", AccessibilityNoOpPredicate,
               AccessibilityPredicateType::kRowBounds);
    add_to_map(u"COLUMN_BOUNDS", AccessibilityNoOpPredicate,
               AccessibilityPredicateType::kColumnBounds);
    add_to_map(u"TABLE_BOUNDS", AccessibilityNoOpPredicate,
               AccessibilityPredicateType::kTableBounds);

    // These do not have search predicates and are for metrics tracking.
    predicate_to_enum_map[u"UNKNOWN"] = AccessibilityPredicateType::kUnknown;
    predicate_to_enum_map[u"DEFAULT"] = AccessibilityPredicateType::kDefault;

    return std::make_tuple(std::move(search_key_to_predicate_map),
                           std::move(predicate_to_enum_map),
                           base::JoinString(std::move(all_search_keys), u","));
  }());

  return *search_key_data;
}

ui::AccessibilityMatchPredicate PredicateForSearchKey(
    std::u16string& element_type) {
  const auto& search_map =
      std::get<SearchKeyToPredicateMap>(GetSearchKeyData());
  auto it = search_map.find(element_type);
  if (it != search_map.end()) {
    return it->second;
  } else {
    element_type = u"UNKNOWN";
  }

  // If we don't recognize the selector, return any element that a
  // screen reader should navigate to.
  return AllInterestingNodesPredicate;
}

AccessibilityPredicateType EnumForPredicate(
    const std::u16string& element_type) {
  const auto& enum_map = std::get<PredicateToEnumMap>(GetSearchKeyData());
  auto it = enum_map.find(element_type);
  if (it != enum_map.end()) {
    return it->second;
  } else {
    NOTREACHED();
  }
}

// The element in the document for which we may be displaying an autofill popup.
int32_t g_element_hosting_autofill_popup_unique_id = ui::kInvalidAXNodeID;

// The element in the document that is the next element after
// |g_element_hosting_autofill_popup_unique_id|.
int32_t g_element_after_element_hosting_autofill_popup_unique_id =
    ui::kInvalidAXNodeID;

// Autofill popup will not be part of the |AXTree| that is sent by renderer.
// Hence, we need a proxy |AXNode| to represent the autofill popup.
ui::BrowserAccessibility* g_autofill_popup_proxy_node = nullptr;
ui::AXNode* g_autofill_popup_proxy_node_ax_node = nullptr;

void DeleteAutofillPopupProxy() {
  if (g_autofill_popup_proxy_node) {
    delete g_autofill_popup_proxy_node;
    delete g_autofill_popup_proxy_node_ax_node;
    g_autofill_popup_proxy_node = nullptr;
  }
}

// The most common use of the EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY
// API is to retrieve character bounds for one word at a time. There
// should never need to be a reason to return more than this many
// character bounding boxes at once. Set the limit much higher than needed
// but small enough to prevent wasting memory and cpu if abused.
const int kMaxCharacterBoundingBoxLen = 1024;

// This is the value of View.NO_ID, used to mark a View that has no ID.
const int kViewNoId = -1;

std::optional<int> MaybeFindRowColumn(ui::BrowserAccessibility* start_node,
                                      std::u16string element_type,
                                      jboolean forwards) {
  bool want_row = base::EqualsASCII(element_type, kHtmlTypeRow);
  bool want_col = base::EqualsASCII(element_type, kHtmlTypeColumn);
  bool want_row_bounds = base::EqualsASCII(element_type, kHtmlTypeRowBounds);
  bool want_col_bounds = base::EqualsASCII(element_type, kHtmlTypeColumnBounds);
  bool want_table_bounds =
      base::EqualsASCII(element_type, kHtmlTypeTableBounds);
  if (!want_row && !want_col && !want_row_bounds && !want_col_bounds &&
      !want_table_bounds) {
    // The search should continue for other types.
    return std::nullopt;
  }

  // See if we're in a table and grab the cell-like node we're under on the way.
  ui::BrowserAccessibility* table_node = start_node;
  ui::AXNode* cell_node = nullptr;
  int cur_row_index, cur_col_index;
  while (table_node) {
    if (AccessibilityTablePredicate(start_node, table_node)) {
      break;
    }

    auto* node = table_node->node();
    if (std::optional<int> row = node->GetTableCellRowIndex(),
        col = node->GetTableCellColIndex();
        row && col) {
      cell_node = node;
      cur_row_index = *row;
      cur_col_index = *col;
    }

    table_node = table_node->PlatformGetParent();
  }

  if (!table_node) {
    return ui::kInvalidAXNodeID;
  }

  ui::AXTree* tree = start_node->node()->tree();
  ui::AXTableInfo* table_info = tree->GetTableInfo(table_node->node());
  if (!table_info) {
    return ui::kInvalidAXNodeID;
  }

  // Nothing more to do if the table is empty.
  if (table_info->cell_ids.empty()) {
    return ui::kInvalidAXNodeID;
  }

  // This may occur if we're somewhere in the table but not within a cell e.g.
  // on the table node itself. In these cases, try to go to the first cell.
  if (!cell_node) {
    return table_info->cell_ids[0].empty() ? ui::kInvalidAXNodeID
                                           : table_info->cell_ids[0][0];
  }

  // Move in the desired direction by the element type.
  int want_row_index = cur_row_index, want_col_index = cur_col_index;
  if (want_row) {
    want_row_index += forwards ? 1 : -1;
  }

  if (want_col) {
    want_col_index += forwards ? 1 : -1;
  }

  if (want_col_bounds || want_table_bounds) {
    want_row_index = forwards ? table_info->row_count - 1 : 0;
  }

  if (want_row_bounds || want_table_bounds) {
    want_col_index = forwards ? table_info->col_count - 1 : 0;
  }

  // This causes the caller to stop its search and indicate appropriately when
  // trying to move past a boundary.
  if (want_row_index < 0 ||
      static_cast<size_t>(want_row_index) >= table_info->row_count ||
      want_col_index < 0 ||
      static_cast<size_t>(want_col_index) >= table_info->col_count) {
    return ui::kInvalidAXNodeID;
  }

  // This causes the caller to stop its search and indicate appropriately when
  // trying to move to the same cell.
  if ((want_row_bounds && want_col_index == cur_col_index) ||
      (want_col_bounds && want_row_index == cur_row_index) ||
      (want_table_bounds && want_row_index == cur_row_index &&
       want_col_index == cur_col_index)) {
    return ui::kInvalidAXNodeID;
  }

  return table_info->cell_ids[want_row_index][want_col_index];
}

// Converts RangePairs into a pair of int vectors.
std::pair<std::vector<int>, std::vector<int>> ToVectorPair(
    const RangePairs& range_pairs) {
  std::vector<int> starts, ends;
  starts.reserve(range_pairs.size());
  ends.reserve(range_pairs.size());
  for (const auto& range : range_pairs) {
    starts.push_back(range.first);
    ends.push_back(range.second);
  }
  return {starts, ends};
}

template <class CppKeyType,
          class CppConstructableType,
          class JavaType,
          class JniWrapperType>
ScopedJavaLocalRef<jobject> ToJavaRangesMap(
    JNIEnv* env,
    const std::optional<absl::flat_hash_map<CppKeyType, RangePairs>>&
        text_style_map,
    void (*SetJavaMapValue)(JNIEnv*,
                            const jni_zero::JavaRef<jobject>&,
                            JniWrapperType,
                            const jni_zero::JavaRef<jintArray>&,
                            const jni_zero::JavaRef<jintArray>&),
    base::RepeatingCallback<JavaType(CppConstructableType)> to_java_map_key,
    int* ranges_count) {
  if (!text_style_map) {
    return nullptr;
  }
  // Due to type erasure, the map key type is always `jobject`, so we must make
  // sure to call with the correct actual key type.
  ScopedJavaLocalRef<jobject> java_map =
      Java_AccessibilityNodeInfoUtils_createTextAttributeRangesMap(env);
  for (const auto& entry : *text_style_map) {
    JavaType java_map_key = to_java_map_key.Run(entry.first);
    std::pair<std::vector<int>, std::vector<int>> pair =
        ToVectorPair(entry.second);
    auto java_starts = ToJavaIntArray(env, pair.first);
    auto java_ends = ToJavaIntArray(env, pair.second);
    SetJavaMapValue(env, java_map, std::move(java_map_key), java_starts,
                    java_ends);
    if (ranges_count) {
      *ranges_count += entry.second.size();
    }
  }
  return java_map;
}

ScopedJavaLocalRef<jobject> ToJavaFloatRangesMap(
    JNIEnv* env,
    const std::optional<absl::flat_hash_map<float, RangePairs>>& text_style_map,
    int* ranges_count) {
  return ToJavaRangesMap(
      env, text_style_map,
      &Java_AccessibilityNodeInfoUtils_setTextAttributeRangesMapFloatValue,
      base::BindRepeating(
          [](float value) { return static_cast<jfloat>(value); }),
      ranges_count);
}

template <class T>
ScopedJavaLocalRef<jobject> ToJavaIntRangesMap(
    JNIEnv* env,
    const std::optional<absl::flat_hash_map<T, RangePairs>>& text_style_map,
    int* ranges_count) {
  return ToJavaRangesMap(
      env, text_style_map,
      &Java_AccessibilityNodeInfoUtils_setTextAttributeRangesMapIntValue,
      base::BindRepeating([](T value) { return static_cast<jint>(value); }),
      ranges_count);
}

ScopedJavaLocalRef<jobject> ToJavaStringRangesMap(
    JNIEnv* env,
    const std::optional<absl::flat_hash_map<std::u16string, RangePairs>>&
        text_style_map,
    int* ranges_count) {
  return ToJavaRangesMap(
      env, text_style_map,
      &Java_AccessibilityNodeInfoUtils_setTextAttributeRangesMapStringValue,
      base::BindRepeating(&base::android::ConvertUTF16ToJavaString,
                          base::Unretained(env)),
      ranges_count);
}

}  // anonymous namespace

class WebContentsAccessibilityAndroid::Connector
    : public RenderWidgetHostConnector {
 public:
  Connector(WebContents* web_contents,
            WebContentsAccessibilityAndroid* accessibility);
  ~Connector() override = default;

  void DeleteEarly();

  // RenderWidgetHostConnector:
  void UpdateRenderProcessConnection(
      RenderWidgetHostViewAndroid* old_rwhva,
      RenderWidgetHostViewAndroid* new_rhwva) override;

 private:
  std::unique_ptr<WebContentsAccessibilityAndroid> accessibility_;
};

WebContentsAccessibilityAndroid::Connector::Connector(
    WebContents* web_contents,
    WebContentsAccessibilityAndroid* accessibility)
    : RenderWidgetHostConnector(web_contents), accessibility_(accessibility) {
  Initialize();
}

void WebContentsAccessibilityAndroid::Connector::DeleteEarly() {
  RenderWidgetHostConnector::DestroyEarly();
}

void WebContentsAccessibilityAndroid::Connector::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva) {
    old_rwhva->SetWebContentsAccessibility(nullptr);
  }
  if (new_rwhva) {
    new_rwhva->SetWebContentsAccessibility(accessibility_.get());
  }
  accessibility_->UpdateBrowserAccessibilityManager();
}

WebContentsAccessibilityAndroid::WebContentsAccessibilityAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    WebContents* web_contents,
    const JavaParamRef<jobject>& jaccessibility_node_info_builder)
    : java_ref_(env, obj),
      java_anib_ref_(env, jaccessibility_node_info_builder),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      frame_info_initialized_(false) {
  // We must initialize this after weak_ptr_factory_ because it can result in
  // calling UpdateBrowserAccessibilityManager() which accesses
  // weak_ptr_factory_.
  connector_ = new Connector(web_contents, this);
}

WebContentsAccessibilityAndroid::WebContentsAccessibilityAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong ax_tree_update_ptr,
    const JavaParamRef<jobject>& jaccessibility_node_info_builder)
    : java_ref_(env, obj),
      java_anib_ref_(env, jaccessibility_node_info_builder),
      web_contents_(nullptr),
      frame_info_initialized_(false) {
  std::unique_ptr<ui::AXTreeUpdate> ax_tree_snapshot(
      reinterpret_cast<ui::AXTreeUpdate*>(ax_tree_update_ptr));
  snapshot_root_manager_ = std::make_unique<BrowserAccessibilityManagerAndroid>(
      *ax_tree_snapshot, GetWeakPtr(), *this, nullptr);
  snapshot_root_manager_->BuildAXTreeHitTestCache();
  connector_ = nullptr;
}

WebContentsAccessibilityAndroid::WebContentsAccessibilityAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jassist_data_builder,
    WebContents* web_contents)
    : java_ref_(env, obj),
      java_adb_ref_(env, jassist_data_builder),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)) {
  // A Connector is not required for a simple snapshot.
  connector_ = nullptr;
}

WebContentsAccessibilityAndroid::~WebContentsAccessibilityAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  // Clean up autofill popup proxy node in case the popup was not dismissed.
  DeleteAutofillPopupProxy();

  Java_WebContentsAccessibilityImpl_onNativeObjectDestroyed(env, obj);
}

ui::AXPlatformNodeId WebContentsAccessibilityAndroid::GetOrCreateAXNodeUniqueId(
    ui::AXNodeID ax_node_id) {
  // Per-tab uniqueness is not necessary in snapshots, so return the blink node
  // id.
  return ui::AXPlatformNodeId(MakePassKey(), ax_node_id);
}

void WebContentsAccessibilityAndroid::OnAXNodeDeleted(ui::AXNodeID ax_node_id) {
}

void WebContentsAccessibilityAndroid::ConnectInstanceToRootManager(
    JNIEnv* env) {
  BrowserAccessibilityManagerAndroid* manager =
      GetRootBrowserAccessibilityManager();

  if (manager) {
    manager->set_web_contents_accessibility(GetWeakPtr());
  }
}

void WebContentsAccessibilityAndroid::UpdateBrowserAccessibilityManager() {
  BrowserAccessibilityManagerAndroid* manager =
      GetRootBrowserAccessibilityManager();
  if (manager) {
    manager->set_web_contents_accessibility(GetWeakPtr());
  }
}

void WebContentsAccessibilityAndroid::DeleteEarly(JNIEnv* env) {
  if (connector_) {
    connector_->DeleteEarly();
  } else {
    delete this;
  }
}

void WebContentsAccessibilityAndroid::DisableRendererAccessibility(
    JNIEnv* env) {
  // This method should only be called when |snapshot_root_manager_| is null,
  // which means this instance was constructed via a web contents and not an
  // AXTreeUpdate (e.g. for snapshots, frozen tabs, paint preview, etc).
  DCHECK(!snapshot_root_manager_);

  // To disable the renderer, the root manager /should/ already be connected to
  // this instance, and we need to reset the weak pointer it has to |this|. In
  // some rare cases, such as if a user rapidly toggles accessibility on/off,
  // a manager may not be connected, in which case a reset is not needed.
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (root_manager) {
    root_manager->ResetWebContentsAccessibility();
  }

  // The local cache of Java strings can be cleared, and we should reset any
  // local state variables. The Connector should continue to live, since we want
  // the RFHI to still have access to this object for possible re-enables,
  // or frame notifications.
  common_string_cache_.clear();
  ResetContentChangedEventsCounter();

  // Turn off accessibility on the renderer side by resetting the AXMode.
  scoped_accessibility_mode_.reset();
}

void WebContentsAccessibilityAndroid::ReEnableRendererAccessibility(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  // This method should only be called when |snapshot_root_manager_| is null,
  // which means this instance was constructed via a web contents and not an
  // AXTreeUpdate (e.g. for snapshots, frozen tabs, paint preview, etc).
  DCHECK(!snapshot_root_manager_);

  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  // A request to re-enable renderer accessibility implies AT use on the
  // Java-side, so we need to set the root manager's reference to |this| to
  // rebuild the C++ -> Java bridge. The web contents may have changed, so
  // update the reference just in case.
  web_contents_ = static_cast<WebContentsImpl*>(web_contents);

  // If we are re-enabling, the root manager may already be connected, in which
  // case we can set its weak pointer to |this|. However, if a user has rapidly
  // turned accessibility on/off, the manager may not be ready. If the manager
  // is not ready, the framework will continue polling until it is connected.
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (root_manager) {
    root_manager->set_web_contents_accessibility(GetWeakPtr());
  }
}

void WebContentsAccessibilityAndroid::SetBrowserAXMode(
    JNIEnv* env,
    jboolean is_known_screen_reader_enabled,
    jboolean is_complex_accessibility_service_enabled,
    jboolean is_form_controls_candidate,
    jboolean is_on_screen_mode_candidate) {
  // Set the AXMode based on currently running services, sent from Java-side
  // code, in the following priority:
  //
  // (Note: This must /always/ take the top priority.)
  // 1. If the performance filtering enterprise policy is disabled, then the
  // mode must be the most complete mode possible (i.e. no performance
  // optimizations). (ui::kAXModeComplete)
  //
  // 2. When a known screen reader is running, use
  // ui::kAXModeComplete | ui::AXMode::kAXModeScreenReader.
  //     2a. (Experimental) If a known screen reader is the only service
  //     running, then this is a candidate to use the "on screen only"
  //     experimental mode. (ui::kAXModeOnScreen | ui::kAXModeScreenReader)
  //
  // 3. When services are running that require detailed information according to
  // the Java-side code (i.e. a "complex" accessibility service), use the
  // complete mode. (ui::kAXModeComplete)
  //
  // 4. When the only services running are password managers, then this is a
  // candidate for filtering for only form controls. (ui::kAXModeFormControls)
  //
  // 5. As a final case - at least one accessibility service must be enabled,
  // and the union of all services has not requested enough information to be in
  // a complete state, but has requested more than a limited state such as form
  // controls, so fallback to a basic state. (ui::kAXModeBasic)
  //
  // TODO(crbug.com/413016129): Consider adding the ability to turn off the
  // AXMode here by sending whether or not any service is running. This case is
  // currently handled by the AutoDisableAccessibilityHandler, which waits ~5
  // seconds before turning off accessibility, to account for quick toggling of
  // the state. Analysis of existing data could show whether the 5 second wait
  // is necessary.
  BrowserAccessibilityStateImpl* accessibility_state =
      BrowserAccessibilityStateImpl::GetInstance();
  ui::AXMode target_mode;
  if (!accessibility_state->IsPerformanceFilteringAllowed()) {
    // Adds kScreenReader to ensure no filtering via the non-screen-reader case.
    target_mode = ui::kAXModeComplete | ui::AXMode::kScreenReader;
  } else if (is_known_screen_reader_enabled) {
    target_mode = ui::kAXModeComplete | ui::AXMode::kScreenReader;
    if (is_on_screen_mode_candidate &&
        features::IsAccessibilityOnScreenAXModeEnabled()) {
      target_mode = ui::kAXModeOnScreen | ui::AXMode::kScreenReader;
    }
  } else if (is_complex_accessibility_service_enabled) {
    target_mode = ui::kAXModeComplete;
  } else if (is_form_controls_candidate) {
    target_mode = ui::kAXModeFormControls;
  } else {
    target_mode = ui::kAXModeBasic;
  }

  target_mode |= ui::AXMode::kFromPlatform;

  scoped_accessibility_mode_ =
      accessibility_state->CreateScopedModeForProcess(target_mode);
}

jboolean WebContentsAccessibilityAndroid::IsRootManagerConnected(JNIEnv* env) {
  return !!GetRootBrowserAccessibilityManager();
}

void WebContentsAccessibilityAndroid::SetAllowImageDescriptions(
    JNIEnv* env,
    jboolean allow_image_descriptions) {
  allow_image_descriptions_ = allow_image_descriptions;
}

BrowserAccessibilityAndroid*
WebContentsAccessibilityAndroid::GetAccessibilityFocus() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return nullptr;
  }
  jint id = Java_WebContentsAccessibilityImpl_getAccessibilityFocusId(env, obj);
  return GetAXFromUniqueID(id);
}

void WebContentsAccessibilityAndroid::HandleContentChanged(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  // If there are a large number of changes it's too expensive to fire all of
  // them, so we just fire one on the root instead.
  content_changed_events_++;
  if (content_changed_events_ < max_content_changed_events_to_fire_) {
    // If it's less than the max event count, fire the event on the specific
    // node that changed.
    Java_WebContentsAccessibilityImpl_handleContentChanged(env, obj, unique_id);
  } else if (content_changed_events_ == max_content_changed_events_to_fire_) {
    // If it's equal to the max event count, fire the event on the
    // root instead.
    BrowserAccessibilityManagerAndroid* root_manager =
        GetRootBrowserAccessibilityManager();
    if (root_manager) {
      auto* root_node = static_cast<BrowserAccessibilityAndroid*>(
          root_manager->GetBrowserAccessibilityRoot());
      if (root_node) {
        Java_WebContentsAccessibilityImpl_handleContentChanged(
            env, obj, root_node->GetUniqueId());
      }
    }
  }
}

void WebContentsAccessibilityAndroid::HandleFocusChanged(
    int32_t unique_id,
    bool is_root_or_frame_root) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleFocusChanged(env, obj, unique_id,
                                                       is_root_or_frame_root);
}

void WebContentsAccessibilityAndroid::HandleCheckStateChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleCheckStateChanged(env, obj,
                                                            unique_id);
}

void WebContentsAccessibilityAndroid::HandleClicked(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleClicked(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::HandleMenuOpened(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_handleMenuOpened(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::HandleWindowContentChange(
    int32_t unique_id,
    int32_t subType) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleWindowContentChange(
      env, obj, unique_id, subType);
}

void WebContentsAccessibilityAndroid::HandleScrollPositionChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleScrollPositionChanged(env, obj,
                                                                unique_id);
}

void WebContentsAccessibilityAndroid::HandleScrolledToAnchor(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleScrolledToAnchor(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::HandlePaneOpened(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_handlePaneOpened(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::HandleLiveRegionNodeChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_handleLiveRegionNodeChanged(env, obj,
                                                                unique_id);
}

void WebContentsAccessibilityAndroid::HandleDefaultActionVerbChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_handleDefaultActionVerbChanged(env, obj,
                                                                    unique_id);
}

void WebContentsAccessibilityAndroid::AnnounceLiveRegionText(
    const std::u16string& text) {
  CHECK(!base::FeatureList::IsEnabled(
      features::kAccessibilityDeprecateTypeAnnounce))
      << "No views should be forcing an announcement outside approved "
         "instances.";

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  // Do not announce empty text.
  if (text.empty()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_announceLiveRegionText(
      env, obj, base::android::ConvertUTF16ToJavaString(env, text));
}

void WebContentsAccessibilityAndroid::HandleTextSelectionChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleTextSelectionChanged(env, obj,
                                                               unique_id);
}

void WebContentsAccessibilityAndroid::HandleEditableTextChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleEditableTextChanged(env, obj,
                                                              unique_id);
}

void WebContentsAccessibilityAndroid::HandleActiveDescendantChanged(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  int active_descendant_id_attribute =
      node->GetIntAttribute(ax::mojom::IntAttribute::kActivedescendantId);
  ui::BrowserAccessibility* active_descendant_element =
      node->manager()->GetFromID(active_descendant_id_attribute);

  int active_descendant_id = kViewNoId;
  if (active_descendant_element) {
    active_descendant_id =
        static_cast<BrowserAccessibilityAndroid*>(active_descendant_element)
            ->GetUniqueId();
  }

  Java_WebContentsAccessibilityImpl_handleActiveDescendantChanged(
      env, obj, unique_id, active_descendant_id);
}

void WebContentsAccessibilityAndroid::SignalEndOfTestForTesting(JNIEnv* env) {
  ui::BrowserAccessibilityManager* manager =
      web_contents_->GetRootBrowserAccessibilityManager();
  manager->SignalEndOfTest();
}

void WebContentsAccessibilityAndroid::HandleEndOfTestSignal() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_handleEndOfTestSignal(env, obj);
}

void WebContentsAccessibilityAndroid::HandleSliderChanged(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleSliderChanged(env, obj, unique_id);
}

void WebContentsAccessibilityAndroid::SendDelayedWindowContentChangedEvent() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_sendDelayedWindowContentChangedEvent(env,
                                                                         obj);
}

void WebContentsAccessibilityAndroid::HandleHover(int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleHover(env, obj, unique_id);
}

bool WebContentsAccessibilityAndroid::OnHoverEvent(
    const ui::MotionEventAndroid& event) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return false;
  }

  if (!Java_WebContentsAccessibilityImpl_onHoverEvent(
          env, obj,
          ui::MotionEventAndroid::GetAndroidAction(event.GetAction()))) {
    return false;
  }

  // |HitTest| sends an IPC to the render process to do the hit testing.
  // The response is handled by HandleHover when it returns.
  // Hover event was consumed by accessibility by now. Return true to
  // stop the event from proceeding.
  if (event.GetAction() != ui::MotionEvent::Action::HOVER_EXIT &&
      GetRootBrowserAccessibilityManager()) {
    gfx::PointF point = event.GetPointPix();
    point.Scale(1 / page_scale_);
    GetRootBrowserAccessibilityManager()->HitTest(gfx::ToFlooredPoint(point),
                                                  /*request_id=*/0);
  }

  return true;
}

bool WebContentsAccessibilityAndroid::OnHoverEventNoRenderer(JNIEnv* env,
                                                             jfloat x,
                                                             jfloat y) {
  gfx::PointF point = gfx::PointF(x, y);
  if (BrowserAccessibilityManagerAndroid* root_manager =
          GetRootBrowserAccessibilityManager()) {
    auto* hover_node = static_cast<BrowserAccessibilityAndroid*>(
        root_manager->ApproximateHitTest(gfx::ToFlooredPoint(point)));
    if (hover_node &&
        hover_node != root_manager->GetBrowserAccessibilityRoot()) {
      HandleHover(hover_node->GetUniqueId());
      return true;
    }
  }
  return false;
}

void WebContentsAccessibilityAndroid::HandleNavigate(int32_t root_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_WebContentsAccessibilityImpl_handleNavigate(env, obj, root_id);
}

void WebContentsAccessibilityAndroid::UpdateMaxNodesInCache() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_updateMaxNodesInCache(env, obj);
}

void WebContentsAccessibilityAndroid::ClearNodeInfoCacheForGivenId(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_clearNodeInfoCacheForGivenId(env, obj,
                                                                 unique_id);
}

std::u16string
WebContentsAccessibilityAndroid::GenerateAccessibilityNodeInfoString(
    int32_t unique_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return {};
  }

  return base::android::ConvertJavaStringToUTF16(
      Java_WebContentsAccessibilityImpl_generateAccessibilityNodeInfoString(
          env, obj, unique_id));
}

base::android::ScopedJavaLocalRef<jstring>
WebContentsAccessibilityAndroid::GetSupportedHtmlElementTypes(JNIEnv* env) {
  const std::u16string& all_keys = std::get<std::u16string>(GetSearchKeyData());
  return GetCanonicalJNIString(env, all_keys).AsLocalRef(env);
}

WebContentsAccessibilityAndroid::WebContentsAccessibilityAndroid() = default;

jint WebContentsAccessibilityAndroid::GetRootId(JNIEnv* env) {
  if (BrowserAccessibilityManagerAndroid* root_manager =
          GetRootBrowserAccessibilityManager()) {
    auto* root = static_cast<BrowserAccessibilityAndroid*>(
        root_manager->GetBrowserAccessibilityRoot());
    if (root) {
      return static_cast<jint>(root->GetUniqueId());
    }
  }
  return ui::kAXAndroidInvalidViewId;
}

jboolean WebContentsAccessibilityAndroid::IsNodeValid(JNIEnv* env,
                                                      jint unique_id) {
  return GetAXFromUniqueID(unique_id) != nullptr;
}

void WebContentsAccessibilityAndroid::HitTest(JNIEnv* env, jint x, jint y) {
  if (BrowserAccessibilityManagerAndroid* root_manager =
          GetRootBrowserAccessibilityManager()) {
    root_manager->HitTest(gfx::Point(x, y), /*request_id=*/0);
  }
}

jboolean WebContentsAccessibilityAndroid::IsEditableText(JNIEnv* env,
                                                         jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  return node->IsTextField();
}

jboolean WebContentsAccessibilityAndroid::IsFocused(JNIEnv* env,
                                                    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  return node->IsFocused();
}

jint WebContentsAccessibilityAndroid::GetEditableTextSelectionStart(
    JNIEnv* env,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  return node->GetSelectionStart();
}

jint WebContentsAccessibilityAndroid::GetEditableTextSelectionEnd(
    JNIEnv* env,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  return node->GetSelectionEnd();
}

ScopedJavaLocalRef<jintArray>
WebContentsAccessibilityAndroid::GetAbsolutePositionForNode(JNIEnv* env,
                                                            jint unique_id) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return nullptr;
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return nullptr;
  }

  float dip_scale = 1 / root_manager->device_scale_factor();
  gfx::Rect absolute_rect = gfx::ScaleToEnclosingRect(
      node->GetUnclippedRootFrameBoundsRect(), dip_scale, dip_scale);
  int rect[] = {absolute_rect.x(), absolute_rect.y(), absolute_rect.right(),
                absolute_rect.bottom()};
  return ToJavaIntArray(env, rect);
}

static size_t ActualUnignoredChildCount(const ui::AXNode* node) {
  size_t count = 0;
  for (const ui::AXNode* child : node->children()) {
    if (child->IsIgnored()) {
      count += ActualUnignoredChildCount(child);
    } else {
      ++count;
    }
  }
  return count;
}

void WebContentsAccessibilityAndroid::UpdateAccessibilityNodeInfoBoundsRect(
    JNIEnv* env,
    const ScopedJavaLocalRef<jobject>& obj,
    const JavaParamRef<jobject>& info,
    jint unique_id,
    BrowserAccessibilityAndroid* node) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return;
  }

  ui::AXOffscreenResult offscreen_result = ui::AXOffscreenResult::kOnscreen;
  float dip_scale = 1 / root_manager->device_scale_factor();
  gfx::Rect absolute_rect = gfx::ScaleToEnclosingRect(
      node->GetUnclippedRootFrameBoundsRect(&offscreen_result), dip_scale,
      dip_scale);
  gfx::Rect parent_relative_rect = absolute_rect;
  if (node->PlatformGetParent()) {
    gfx::Rect parent_rect = gfx::ScaleToEnclosingRect(
        node->PlatformGetParent()->GetUnclippedRootFrameBoundsRect(), dip_scale,
        dip_scale);
    parent_relative_rect.Offset(-parent_rect.OffsetFromOrigin());
  }
  bool is_offscreen = offscreen_result == ui::AXOffscreenResult::kOffscreen;

  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoLocation(
      env, obj, info, unique_id, absolute_rect.x(), absolute_rect.y(),
      parent_relative_rect.x(), parent_relative_rect.y(), absolute_rect.width(),
      absolute_rect.height(), is_offscreen);
}

jboolean WebContentsAccessibilityAndroid::UpdateCachedAccessibilityNodeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    jint unique_id) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return false;
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  ScopedJavaLocalRef<jobject> obj = java_anib_ref_.get(env);
  if (obj.is_null()) {
    return false;
  }

  // Update cached nodes by providing new enclosing Rects
  UpdateAccessibilityNodeInfoBoundsRect(env, obj, info, unique_id, node);

  return true;
}

void WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfoChildIds(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    const ScopedJavaLocalRef<jobject>& obj,
    BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  // Build a vector of child ids
  std::vector<int> child_ids;
  for (const auto& child : node->PlatformChildren()) {
    const auto& android_node =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    child_ids.push_back(android_node.GetUniqueId());
  }
  if (child_ids.size()) {
    Java_AccessibilityNodeInfoBuilder_addAccessibilityNodeInfoChildren(
        env, obj, info, ToJavaIntArray(env, child_ids));
  }
}

void WebContentsAccessibilityAndroid::
    PopulateAccessibilityNodeInfoBooleanAttributes(
        JNIEnv* env,
        const JavaParamRef<jobject>& info,
        const ScopedJavaLocalRef<jobject>& obj,
        BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  jint unique_id = node->GetUniqueId();
  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoBooleanAttributes(
      env, obj, info, unique_id, node->IsCheckable(), node->IsClickable(),
      node->IsContentInvalid(), node->IsEnabled(), node->IsEditable(),
      node->IsFocusable(), node->IsFocused(), node->HasImage(),
      node->IsPasswordField(), node->IsScrollable(), node->IsSelected(),
      node->IsVisibleToUser(), node->HasCharacterLocations(),
      node->IsRequired(), node->IsHeading() || node->IsTableHeader(),
      node->HasLayoutBasedActions());
}

void WebContentsAccessibilityAndroid::
    PopulateAccessibilityNodeInfoActionAttributes(
        JNIEnv* env,
        const JavaParamRef<jobject>& info,
        const ScopedJavaLocalRef<jobject>& obj,
        BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  jint unique_id = node->GetUniqueId();
  Java_AccessibilityNodeInfoBuilder_addAccessibilityNodeInfoActions(
      env, obj, info, unique_id, node->CanScrollForward(),
      node->CanScrollBackward(), node->CanScrollUp(), node->CanScrollDown(),
      node->CanScrollLeft(), node->CanScrollRight(), node->IsClickable(),
      node->IsTextField(), node->IsEnabled(), node->IsEditable(),
      node->IsFocusable(), node->IsFocused(), node->IsCollapsed(),
      node->IsExpanded(), node->HasNonEmptyValue(),
      !node->GetAccessibleNameUTF16().empty(), node->IsSeekControl(),
      node->IsFormDescendant());
}

void WebContentsAccessibilityAndroid::
    PopulateAccessibilityNodeInfoBaseAttributes(
        JNIEnv* env,
        const JavaParamRef<jobject>& info,
        const ScopedJavaLocalRef<jobject>& obj,
        BrowserAccessibilityAndroid* node,
        int parent_id) {
  CHECK(!obj.is_null());

  jint unique_id = node->GetUniqueId();
  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoBaseAttributes(
      env, obj, info, unique_id, parent_id,
      GetCanonicalJNIString(env, node->GetClassName()),
      GetCanonicalJNIString(env, node->GetRoleString()),
      GetCanonicalJNIString(env, node->GetRoleDescription()),
      base::android::ConvertUTF16ToJavaString(env, node->GetHint()),
      base::android::ConvertUTF16ToJavaString(env, node->GetTooltipText()),
      base::android::ConvertUTF16ToJavaString(env, node->GetTargetUrl()),
      node->CanOpenPopup(), node->IsMultiLine(), node->AndroidInputType(),
      node->AndroidLiveRegionType(),
      GetCanonicalJNIString(env, node->GetContentInvalidErrorMessage()),
      node->ClickableScore(), GetCanonicalJNIString(env, node->GetCSSDisplay()),
      base::android::ConvertUTF16ToJavaString(env, node->GetBrailleLabel()),
      GetCanonicalJNIString(env, node->GetBrailleRoleDescription()),
      node->ExpandedState(), node->GetChecked(),
      base::android::ToJavaIntArray(env, node->GetLabelledByAndroidIds()));
}

void WebContentsAccessibilityAndroid::
    PopulateAccessibilityNodeInfoTextWithFormatting(
        JNIEnv* env,
        const JavaParamRef<jobject>& info,
        const ScopedJavaLocalRef<jobject>& obj,
        BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  bool is_link = ui::IsLink(node->GetRole());
  TextFormattingMetricsRecorder recorder;
  recorder.StartTimer(TextFormattingMetric::kTotalDuration);

  recorder.StartTimer(TextFormattingMetric::kCheckAXFocusDuration);
  std::unique_ptr<AXStyleData> style_data;
  if (IsAccessibilityFocused(node)) {
    style_data = std::make_unique<AXStyleData>();
  }
  recorder.StopTimer(TextFormattingMetric::kCheckAXFocusDuration);

  recorder.StartTimer(TextFormattingMetric::kGetTextContentDuration);
  std::u16string text = node->GetSubstringTextContentUTF16(
      /*min_length=*/std::nullopt, style_data.get());
  recorder.StopTimer(TextFormattingMetric::kGetTextContentDuration);

  recorder.StartTimer(TextFormattingMetric::kToJavaDataDuration);
  ScopedJavaLocalRef<jobject> java_suggestions;
  ScopedJavaLocalRef<jobject> java_links;
  ScopedJavaLocalRef<jobject> java_text_sizes;
  ScopedJavaLocalRef<jobject> java_text_styles;
  ScopedJavaLocalRef<jobject> java_text_positions;
  ScopedJavaLocalRef<jobject> java_fg_colors;
  ScopedJavaLocalRef<jobject> java_bg_colors;
  ScopedJavaLocalRef<jobject> java_font_families;
  ScopedJavaLocalRef<jobject> java_locales;

  int ranges_count = 0;
  if (style_data) {
    java_suggestions =
        ToJavaStringRangesMap(env, style_data->suggestions, &ranges_count);
    java_links = ToJavaStringRangesMap(env, style_data->links, &ranges_count);
    java_text_sizes =
        ToJavaFloatRangesMap(env, style_data->text_sizes, &ranges_count);
    java_text_styles =
        ToJavaIntRangesMap(env, style_data->text_styles, &ranges_count);
    java_text_positions =
        ToJavaIntRangesMap(env, style_data->text_positions, &ranges_count);
    java_fg_colors =
        ToJavaIntRangesMap(env, style_data->foreground_colors, &ranges_count);
    java_bg_colors =
        ToJavaIntRangesMap(env, style_data->background_colors, &ranges_count);
    java_font_families = ToJavaCanonicalStringRangesMap(
        env, style_data->font_families, &ranges_count);
    java_locales =
        ToJavaCanonicalStringRangesMap(env, style_data->locales, &ranges_count);
  }
  recorder.StopTimer(TextFormattingMetric::kToJavaDataDuration);

  recorder.StartTimer(TextFormattingMetric::kSetAniTextDuration);
  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoText(
      env, obj, info, base::android::ConvertUTF16ToJavaString(env, text),
      is_link, node->IsTextField(),
      base::android::ConvertUTF16ToJavaString(env, node->GetStateDescription()),
      base::android::ConvertUTF16ToJavaString(env, node->GetContainerTitle()),
      base::android::ConvertUTF16ToJavaString(env,
                                              node->GetContentDescription()),
      base::android::ConvertUTF16ToJavaString(
          env, node->GetSupplementalDescription()),
      java_suggestions, java_links, java_text_sizes, java_text_styles,
      java_text_positions, java_fg_colors, java_bg_colors, java_font_families,
      java_locales);
  recorder.StopTimer(TextFormattingMetric::kSetAniTextDuration);
  recorder.StopTimer(TextFormattingMetric::kTotalDuration);

  recorder.EmitHistograms(text.length(), !!style_data);
  if (style_data) {
    RecordTextFormattingRangeCountsForTextLengthHistogram(text, ranges_count);
    RecordTextFormattingDurationForRangeCountHistogram(
        ranges_count, recorder.GetTotalDuration());
  }
}

void WebContentsAccessibilityAndroid::
    PopulateAccessibilityNodeInfoTextWithoutFormatting(
        JNIEnv* env,
        const JavaParamRef<jobject>& info,
        const ScopedJavaLocalRef<jobject>& obj,
        BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  bool is_link = ui::IsLink(node->GetRole());
  ScopedJavaLocalRef<jintArray> java_suggestion_starts;
  ScopedJavaLocalRef<jintArray> java_suggestion_ends;
  ScopedJavaLocalRef<jobjectArray> java_suggestion_text;
  std::vector<int> suggestion_starts;
  std::vector<int> suggestion_ends;
  node->GetSuggestions(&suggestion_starts, &suggestion_ends);

  TextFormattingMetricsRecorder recorder;
  // Don't include the time taken to get suggestions in total duration here,
  // since it's not included in the other code path.
  recorder.StartTimer(TextFormattingMetric::kTotalDuration);

  recorder.StartTimer(TextFormattingMetric::kCheckAXFocusDuration);
  bool would_have_style_data = IsAccessibilityFocused(node);
  recorder.StopTimer(TextFormattingMetric::kCheckAXFocusDuration);

  recorder.StartTimer(TextFormattingMetric::kToJavaDataDuration);
  if (suggestion_starts.size() && suggestion_ends.size()) {
    java_suggestion_starts = ToJavaIntArray(env, suggestion_starts);
    java_suggestion_ends = ToJavaIntArray(env, suggestion_ends);

    // TODO: crbug.com/425974312 - Currently we don't retrieve the text of
    // each suggestion, so store a blank string for now.
    std::vector<std::string> suggestion_text(suggestion_starts.size());
    java_suggestion_text =
        base::android::ToJavaArrayOfStrings(env, suggestion_text);
  }
  recorder.StopTimer(TextFormattingMetric::kToJavaDataDuration);

  recorder.StartTimer(TextFormattingMetric::kGetTextContentDuration);
  std::u16string text = node->GetTextContentUTF16();
  recorder.StopTimer(TextFormattingMetric::kGetTextContentDuration);

  recorder.StartTimer(TextFormattingMetric::kSetAniTextDuration);
  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoText(
      env, obj, info, base::android::ConvertUTF16ToJavaString(env, text),
      is_link
          ? base::android::ConvertUTF16ToJavaString(env, node->GetTargetUrl())
          : base::android::ConvertUTF16ToJavaString(env, std::u16string()),
      is_link, node->IsTextField(),
      GetCanonicalJNIString(env, node->GetInheritedString16Attribute(
                                     ax::mojom::StringAttribute::kLanguage)),
      java_suggestion_starts, java_suggestion_ends, java_suggestion_text,
      base::android::ConvertUTF16ToJavaString(env, node->GetStateDescription()),
      base::android::ConvertUTF16ToJavaString(env, node->GetContainerTitle()),
      base::android::ConvertUTF16ToJavaString(env,
                                              node->GetContentDescription()),
      base::android::ConvertUTF16ToJavaString(
          env, node->GetSupplementalDescription()));
  recorder.StopTimer(TextFormattingMetric::kSetAniTextDuration);
  recorder.StopTimer(TextFormattingMetric::kTotalDuration);

  recorder.EmitHistograms(text.length(), would_have_style_data);
}

void WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfoText(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    const ScopedJavaLocalRef<jobject>& obj,
    BrowserAccessibilityAndroid* node) {
  if (::features::IsAccessibilityTextFormattingEnabled()) {
    PopulateAccessibilityNodeInfoTextWithFormatting(env, info, obj, node);
  } else {
    PopulateAccessibilityNodeInfoTextWithoutFormatting(env, info, obj, node);
  }
}

void WebContentsAccessibilityAndroid::
    PopulateAccessibilityNodeInfoViewIdResourceName(
        JNIEnv* env,
        const JavaParamRef<jobject>& info,
        const ScopedJavaLocalRef<jobject>& obj,
        BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());
  std::u16string element_id;
  if (node->GetString16Attribute(ax::mojom::StringAttribute::kHtmlId,
                                 &element_id)) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoViewIdResourceName(
        env, obj, info,
        base::android::ConvertUTF16ToJavaString(env, element_id));
  }
}

void WebContentsAccessibilityAndroid::
    PopulateAccessibilityNodeInfoCollectionInfo(
        JNIEnv* env,
        const JavaParamRef<jobject>& info,
        const ScopedJavaLocalRef<jobject>& obj,
        BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  if (node->IsCollection()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoCollectionInfo(
        env, obj, info,
        /* rowCount= */ node->RowCount(),
        /* columnCount= */ node->ColumnCount(),
        /* isHierarchical= */ node->IsHierarchical(),
        /* selectionMode= */ node->GetSelectionMode());
  }
}

void WebContentsAccessibilityAndroid::
    PopulateAccessibilityNodeInfoCollectionItemInfo(
        JNIEnv* env,
        const JavaParamRef<jobject>& info,
        const ScopedJavaLocalRef<jobject>& obj,
        BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  if (node->IsCollectionItem() || node->IsTableHeader()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoCollectionItemInfo(
        env, obj, info,
        /* rowIndex= */ node->RowIndex(),
        /* rowSpan= */ node->RowSpan(),
        /* columnIndex= */ node->ColumnIndex(),
        /* columnSpan= */ node->ColumnSpan());
  }
}

void WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfoRangeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    const ScopedJavaLocalRef<jobject>& obj,
    BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  // For sliders that are numeric, use the AccessibilityNodeInfo.RangeInfo
  // object as expected. But for non-numeric ranges (e.g. "small", "medium",
  // "large"), do not set the RangeInfo object and instead rely on announcing
  // the aria-valuetext value, which will be included in the node's text value.
  if (node->IsRangeControlWithoutAriaValueText()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoRangeInfo(
        env, obj, info,
        /* rangeType= */ node->AndroidRangeType(),
        /* min= */ node->RangeMin(),
        /* max= */ node->RangeMax(),
        /* current= */ node->RangeCurrentValue());
  }
}

void WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfoPaneTitle(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    const ScopedJavaLocalRef<jobject>& obj,
    BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  if (node->ShouldUsePaneTitle()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoPaneTitle(
        env, obj, info,
        base::android::ConvertUTF16ToJavaString(env, node->GetPaneTitle()));
  }
}

void WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfoSelection(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    const ScopedJavaLocalRef<jobject>& obj,
    BrowserAccessibilityAndroid* node) {
  CHECK(!obj.is_null());

  if (node->IsTextField()) {
    Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoSelectionAttrs(
        env, obj, info,
        /* selectionStart= */ node->GetSelectionStart(),
        /* selectionEnd= */ node->GetSelectionEnd());
  }
}

jboolean WebContentsAccessibilityAndroid::PopulateAccessibilityNodeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    jint unique_id) {
  if (!GetRootBrowserAccessibilityManager()) {
    return false;
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  ScopedJavaLocalRef<jobject> obj = java_anib_ref_.get(env);
  if (obj.is_null()) {
    return false;
  }

  int parent_id = ui::kAXAndroidInvalidViewId;
  auto* parent_node =
      static_cast<BrowserAccessibilityAndroid*>(node->PlatformGetParent());
  if (parent_node) {
    parent_id = parent_node->GetUniqueId();
  }

  PopulateAccessibilityNodeInfoChildIds(env, info, obj, node);
  PopulateAccessibilityNodeInfoBooleanAttributes(env, info, obj, node);
  PopulateAccessibilityNodeInfoActionAttributes(env, info, obj, node);
  PopulateAccessibilityNodeInfoBaseAttributes(env, info, obj, node, parent_id);
  PopulateAccessibilityNodeInfoText(env, info, obj, node);
  PopulateAccessibilityNodeInfoViewIdResourceName(env, info, obj, node);
  PopulateAccessibilityNodeInfoCollectionInfo(env, info, obj, node);
  PopulateAccessibilityNodeInfoCollectionItemInfo(env, info, obj, node);
  PopulateAccessibilityNodeInfoRangeInfo(env, info, obj, node);
  PopulateAccessibilityNodeInfoPaneTitle(env, info, obj, node);
  PopulateAccessibilityNodeInfoSelection(env, info, obj, node);
  UpdateAccessibilityNodeInfoBoundsRect(env, obj, info, unique_id, node);

  return true;
}

jboolean WebContentsAccessibilityAndroid::PopulateAccessibilityEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& event,
    jint unique_id,
    jint event_type) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return false;
  }

  // We will always set boolean, classname, list and scroll attributes.
  Java_WebContentsAccessibilityImpl_setAccessibilityEventBaseAttributes(
      env, obj, event, node->IsChecked(), node->IsEnabled(),
      node->IsPasswordField(), node->IsScrollable(), node->GetItemIndex(),
      node->GetItemCount(), node->GetScrollX(), node->GetScrollY(),
      node->GetMaxScrollX(), node->GetMaxScrollY(),
      GetCanonicalJNIString(env, node->GetClassName()));

  switch (event_type) {
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGED: {
      std::u16string before_text = node->GetTextChangeBeforeText();
      std::u16string text = node->GetTextContentUTF16();
      Java_WebContentsAccessibilityImpl_setAccessibilityEventTextChangedAttrs(
          env, obj, event, node->GetTextChangeFromIndex(),
          node->GetTextChangeAddedCount(), node->GetTextChangeRemovedCount(),
          base::android::ConvertUTF16ToJavaString(env, before_text),
          base::android::ConvertUTF16ToJavaString(env, text));
      break;
    }
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_SELECTION_CHANGED: {
      std::u16string text = node->GetTextContentUTF16();
      Java_WebContentsAccessibilityImpl_setAccessibilityEventSelectionAttrs(
          env, obj, event, node->GetSelectionStart(), node->GetSelectionEnd(),
          node->GetEditableTextLength(),
          base::android::ConvertUTF16ToJavaString(env, text));
      break;
    }
    default:
      break;
  }

  return true;
}

void WebContentsAccessibilityAndroid::Click(JNIEnv* env, jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return;
  }

  // If it's a heading consisting of only a link or a heading nested in a link,
  // click the link.
  BrowserAccessibilityAndroid* heading_link_or_link_heading_node =
      node->GetHeadingLinkOrLinkHeading();
  if (heading_link_or_link_heading_node != nullptr) {
    node = heading_link_or_link_heading_node;
  }

  // Only perform the default action on a node that is enabled. Having the
  // ACTION_CLICK action on the node is not sufficient, since TalkBack won't
  // announce a control as disabled unless it's also marked as clickable, so
  // disabled nodes are secretly clickable if we do not check here.
  // Children of disabled controls/widgets will also have the click action, so
  // ensure that parents/ancestry chain is enabled as well.
  if (node->IsEnabled() && !node->IsDisabledDescendant()) {
    node->manager()->DoDefaultAction(*node);
  }
}

void WebContentsAccessibilityAndroid::Focus(JNIEnv* env, jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->SetFocus(*node);
  }
}

void WebContentsAccessibilityAndroid::Blur(JNIEnv* env) {
  if (BrowserAccessibilityManagerAndroid* root_manager =
          GetRootBrowserAccessibilityManager()) {
    root_manager->SetFocus(*root_manager->GetBrowserAccessibilityRoot());
  }
}

void WebContentsAccessibilityAndroid::ScrollToMakeNodeVisible(JNIEnv* env,
                                                              jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->ScrollToMakeVisible(
        *node, gfx::Rect(node->GetUnclippedFrameBoundsRect().size()));
  }
}

void WebContentsAccessibilityAndroid::SetTextFieldValue(
    JNIEnv* env,
    jint unique_id,
    const JavaParamRef<jstring>& value) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->SetValue(
        *node, base::android::ConvertJavaStringToUTF8(env, value));
  }
}

void WebContentsAccessibilityAndroid::SetSelection(JNIEnv* env,
                                                   jint unique_id,
                                                   jint start,
                                                   jint end) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->SetSelection(ui::BrowserAccessibility::AXRange(
        node->CreatePositionForSelectionAt(start),
        node->CreatePositionForSelectionAt(end)));
  }
}

jboolean WebContentsAccessibilityAndroid::AdjustSlider(JNIEnv* env,
                                                       jint unique_id,
                                                       jboolean increment) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (!android_node->IsSlider() || !android_node->IsEnabled()) {
    return false;
  }

  float value =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
  float min =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
  float max =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
  if (max <= min) {
    return false;
  }

  // If this node has defined a step value, move by that amount. Otherwise, to
  // behave similarly to an Android SeekBar, move by an increment of ~5%.
  float delta;
  if (node->HasFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange)) {
    delta =
        node->GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange);
  } else {
    delta = (max - min) / kDefaultNumberOfTicksForSliders;
  }

  // Add/Subtract based on |increment| boolean, then clamp to range.
  float original_value = value;
  value += (increment ? delta : -delta);
  value = std::clamp(value, min, max);
  if (value != original_value) {
    node->manager()->SetValue(*node, base::NumberToString(value));
    return true;
  }
  return false;
}

void WebContentsAccessibilityAndroid::ShowContextMenu(JNIEnv* env,
                                                      jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->ShowContextMenu(*node);
  }
}

jint WebContentsAccessibilityAndroid::FindElementType(
    JNIEnv* env,
    jint start_id,
    const JavaParamRef<jstring>& element_type_str,
    jboolean forwards,
    jboolean can_wrap_to_last_element,
    jboolean use_default_predicate,
    jboolean is_known_screen_reader_enabled,
    jboolean is_only_one_accessibility_service_enabled) {
  base::ElapsedTimer timer;
  BrowserAccessibilityAndroid* start_node = GetAXFromUniqueID(start_id);
  if (!start_node) {
    return ui::kInvalidAXNodeID;
  }

  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return ui::kInvalidAXNodeID;
  }

  ui::BrowserAccessibility* root = root_manager->GetBrowserAccessibilityRoot();
  if (!root) {
    return ui::kInvalidAXNodeID;
  }

  // If |element_type_str| was empty, we can skip to the default predicate.
  ui::AccessibilityMatchPredicate predicate;
  std::u16string element_type;
  if (use_default_predicate) {
    predicate = AllInterestingNodesPredicate;
    element_type = u"DEFAULT";
  } else {
    element_type =
        base::android::ConvertJavaStringToUTF16(env, element_type_str);
    if (std::optional<int> ret =
            MaybeFindRowColumn(start_node, element_type, forwards);
        ret) {
      ui::BrowserAccessibility* node = start_node->manager()->GetFromID(*ret);
      return node ? static_cast<BrowserAccessibilityAndroid*>(node)
                        ->GetUniqueId()
                  : ui::kInvalidAXNodeID;
    }

    predicate = PredicateForSearchKey(element_type);
  }

  // Record the type of element that was used for navigation, splitting by
  // whether or not a screen reader was running to see how frequently other apps
  // use this functionality.
  if (is_known_screen_reader_enabled &&
      is_only_one_accessibility_service_enabled) {
    base::UmaHistogramEnumeration(
        "Accessibility.Android.FindElementType.Usage2.TalkBack",
        EnumForPredicate(element_type));
  } else if (is_known_screen_reader_enabled) {
    base::UmaHistogramEnumeration(
        "Accessibility.Android.FindElementType.Usage2."
        "TalkBackWithOtherAT",
        EnumForPredicate(element_type));
  } else {
    // When TalkBack isn't running, split by AXMode (for TB we know we will for
    // sure be in a mode with kExtendedProperties).
    BrowserAccessibilityStateImpl* accessibility_state =
        BrowserAccessibilityStateImpl::GetInstance();
    ui::AXMode mode = accessibility_state->GetAccessibilityMode();
    std::string suffix;

    if (mode == ui::kAXModeBasic) {
      suffix = "Basic";
    } else if (mode == ui::kAXModeWebContentsOnly) {
      suffix = "WebContentsOnly";
    } else if (mode == ui::kAXModeFormControls) {
      suffix = "FormControls";
    } else {
      suffix = "Unnamed";
    }

    base::UmaHistogramEnumeration(
        "Accessibility.Android.FindElementType.Usage2.NoTalkBack." + suffix,
        EnumForPredicate(element_type));
  }

  ui::OneShotAccessibilityTreeSearch tree_search(root);
  tree_search.SetStartNode(start_node);
  tree_search.SetDirection(forwards
                               ? ui::OneShotAccessibilityTreeSearch::FORWARDS
                               : ui::OneShotAccessibilityTreeSearch::BACKWARDS);
  tree_search.SetResultLimit(1);
  tree_search.SetImmediateDescendantsOnly(false);
  tree_search.SetCanWrapToLastElement(can_wrap_to_last_element);
  tree_search.SetOnscreenOnly(false);
  tree_search.AddPredicate(predicate);

  if (tree_search.CountMatches() == 0) {
    return ui::kInvalidAXNodeID;
  }

  auto* android_node =
      static_cast<BrowserAccessibilityAndroid*>(tree_search.GetMatchAtIndex(0));
  int32_t element_id = android_node->GetUniqueId();

  // Navigate forwards to the autofill popup's proxy node if focus is currently
  // on the element hosting the autofill popup. Once within the popup, a back
  // press will navigate back to the element hosting the popup. If user swipes
  // past last suggestion in the popup, or swipes left from the first suggestion
  // in the popup, we will navigate to the element that is the next element in
  // the document after the element hosting the popup.
  if (forwards && start_id == g_element_hosting_autofill_popup_unique_id &&
      g_autofill_popup_proxy_node) {
    g_element_after_element_hosting_autofill_popup_unique_id = element_id;
    auto* proxy_android_node =
        static_cast<BrowserAccessibilityAndroid*>(g_autofill_popup_proxy_node);
    return proxy_android_node->GetUniqueId();
  }

  base::UmaHistogramCustomMicrosecondsTimes(
      "Accessibility.Android.Performance.OneShotTreeSearch",
      base::Microseconds(timer.Elapsed().InMicrosecondsF()),
      base::Microseconds(1), base::Microseconds(1000), 80);
  return element_id;
}

jboolean WebContentsAccessibilityAndroid::NextAtGranularity(
    JNIEnv* env,
    jint granularity,
    jboolean extend_selection,
    jint unique_id,
    jint cursor_index) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return false;
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  jint start_index = -1;
  int end_index = -1;
  if (root_manager->NextAtGranularity(granularity, cursor_index, node,
                                      &start_index, &end_index)) {
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null()) {
      return false;
    }
    std::u16string text = node->GetTextContentUTF16();
    Java_WebContentsAccessibilityImpl_finishGranularityMoveNext(
        env, obj, base::android::ConvertUTF16ToJavaString(env, text),
        extend_selection, start_index, end_index);
    return true;
  }
  return false;
}

jint WebContentsAccessibilityAndroid::GetTextLength(JNIEnv* env,
                                                    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return -1;
  }
  std::u16string text = node->GetTextContentUTF16();
  return text.size();
}

void WebContentsAccessibilityAndroid::AddSpellingErrorForTesting(
    JNIEnv* env,
    jint unique_id,
    jint start_offset,
    jint end_offset) {
  ui::BrowserAccessibility* node = GetAXFromUniqueID(unique_id);
  CHECK(node);

  while (node->GetRole() != ax::mojom::Role::kStaticText &&
         node->InternalChildCount() > 0) {
    node = node->InternalChildrenBegin().get();
  }

  CHECK(node->GetRole() == ax::mojom::Role::kStaticText);
  std::u16string text = node->GetTextContentUTF16();
  CHECK_LT(start_offset, static_cast<int>(text.size()));
  CHECK_LE(end_offset, static_cast<int>(text.size()));

  ui::AXNodeData data = node->GetData();
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts,
                           {start_offset});
  data.AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                           {end_offset});
  data.AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int>(ax::mojom::MarkerType::kSuggestion)});
  node->node()->SetData(data);
}

jboolean WebContentsAccessibilityAndroid::PreviousAtGranularity(
    JNIEnv* env,
    jint granularity,
    jboolean extend_selection,
    jint unique_id,
    jint cursor_index) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return false;
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  jint start_index = -1;
  int end_index = -1;
  if (root_manager->PreviousAtGranularity(granularity, cursor_index, node,
                                          &start_index, &end_index)) {
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (obj.is_null()) {
      return false;
    }
    Java_WebContentsAccessibilityImpl_finishGranularityMovePrevious(
        env, obj,
        base::android::ConvertUTF16ToJavaString(env,
                                                node->GetTextContentUTF16()),
        extend_selection, start_index, end_index);
    return true;
  }
  return false;
}

void WebContentsAccessibilityAndroid::RecordInlineTextBoxMetrics(
    bool from_focus) {
  // Track the current AXMode via UMA. We do this here and not in the
  // manager so that we can get a signal of whether we are making requests
  // for modes that don't have kInlineTextBoxes mode flag.
  BrowserAccessibilityStateImpl* accessibility_state =
      BrowserAccessibilityStateImpl::GetInstance();
  ui::AXMode mode = accessibility_state->GetAccessibilityMode();

  ui::AXMode::BundleHistogramValue bundle;
  // Clear out any modes that will confuse the bundle detection.
  mode &= ui::kAXModeComplete | ui::kAXModeFormControls;

  if (mode == ui::kAXModeBasic) {
    bundle = ui::AXMode::BundleHistogramValue::kBasic;
  } else if (mode == ui::kAXModeWebContentsOnly) {
    bundle = ui::AXMode::BundleHistogramValue::kWebContentsOnly;
  } else if (mode == ui::kAXModeComplete) {
    bundle = ui::AXMode::BundleHistogramValue::kComplete;
  } else if (mode == ui::kAXModeFormControls) {
    bundle = ui::AXMode::BundleHistogramValue::kFormControls;
  } else {
    bundle = ui::AXMode::BundleHistogramValue::kUnnamed;
  }

  if (from_focus) {
    base::UmaHistogramEnumeration(
        "Accessibility.Android.InlineTextBoxes.Bundle.FromFocus", bundle);
  } else {
    base::UmaHistogramEnumeration(
        "Accessibility.Android.InlineTextBoxes.Bundle.ExtraData", bundle);
  }
}

void WebContentsAccessibilityAndroid::MoveAccessibilityFocus(
    JNIEnv* env,
    jint old_unique_id,
    jint new_unique_id) {
  BrowserAccessibilityAndroid* old_node = GetAXFromUniqueID(old_unique_id);
  if (old_node) {
    old_node->manager()->ClearAccessibilityFocus(*old_node);
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(new_unique_id);
  if (!node) {
    return;
  }
  node->manager()->SetAccessibilityFocus(*node);

  // When Android sets accessibility focus to a node, we load inline text
  // boxes for that node so that subsequent requests for character bounding
  // boxes will succeed. However, don't do that for the root of the tree,
  // as that will result in loading inline text boxes for the whole tree.
  if (node != node->manager()->GetBrowserAccessibilityRoot()) {
    node->manager()->LoadInlineTextBoxes(*node);
    RecordInlineTextBoxMetrics(/*from_focus=*/true);
  }
}

void WebContentsAccessibilityAndroid::SetSequentialFocusStartingPoint(
    JNIEnv* env,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return;
  }
  node->manager()->SetSequentialFocusNavigationStartingPoint(*node);
}

bool WebContentsAccessibilityAndroid::IsSlider(JNIEnv* env, jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  return node->GetRole() == ax::mojom::Role::kSlider;
}

void WebContentsAccessibilityAndroid::OnAutofillPopupDisplayed(JNIEnv* env) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return;
  }

  ui::BrowserAccessibility* current_focus = root_manager->GetFocus();
  if (!current_focus) {
    return;
  }

  DeleteAutofillPopupProxy();

  // The node is isolated (tree is nullptr) and its id is not important,
  // it should only be not equal to kInvalidAXNodeID to be considered valid.
  ui::AXNodeID id = ~ui::kInvalidAXNodeID;
  g_autofill_popup_proxy_node_ax_node = new ui::AXNode(nullptr, nullptr, id, 0);
  ui::AXNodeData ax_node_data;
  ax_node_data.id = id;
  ax_node_data.role = ax::mojom::Role::kMenu;
  ax_node_data.SetName("Autofill");
  ax_node_data.SetRestriction(ax::mojom::Restriction::kReadOnly);
  ax_node_data.AddState(ax::mojom::State::kFocusable);
  ax_node_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  g_autofill_popup_proxy_node_ax_node->SetData(ax_node_data);
  g_autofill_popup_proxy_node =
      ui::BrowserAccessibility::Create(root_manager,
                                       g_autofill_popup_proxy_node_ax_node)
          .release();

  auto* android_node = static_cast<BrowserAccessibilityAndroid*>(current_focus);

  g_element_hosting_autofill_popup_unique_id = android_node->GetUniqueId();
}

void WebContentsAccessibilityAndroid::OnAutofillPopupDismissed(JNIEnv* env) {
  g_element_hosting_autofill_popup_unique_id = -1;
  g_element_after_element_hosting_autofill_popup_unique_id = -1;
  DeleteAutofillPopupProxy();
}

jint WebContentsAccessibilityAndroid::
    GetIdForElementAfterElementHostingAutofillPopup(JNIEnv* env) {
  if (g_element_after_element_hosting_autofill_popup_unique_id == -1 ||
      !GetAXFromUniqueID(
          g_element_after_element_hosting_autofill_popup_unique_id)) {
    return ui::kInvalidAXNodeID;
  }

  return g_element_after_element_hosting_autofill_popup_unique_id;
}

jboolean WebContentsAccessibilityAndroid::IsAutofillPopupNode(JNIEnv* env,
                                                              jint unique_id) {
  auto* android_node =
      static_cast<BrowserAccessibilityAndroid*>(g_autofill_popup_proxy_node);

  return g_autofill_popup_proxy_node &&
         android_node->GetUniqueId() == unique_id;
}

bool WebContentsAccessibilityAndroid::Scroll(JNIEnv* env,
                                             jint unique_id,
                                             int direction,
                                             bool is_page_scroll) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  return node->Scroll(direction, is_page_scroll);
}

bool WebContentsAccessibilityAndroid::SetRangeValue(JNIEnv* env,
                                                    jint unique_id,
                                                    float value) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (!android_node->GetData().IsRangeValueSupported()) {
    return false;
  }

  float min =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
  float max =
      node->GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
  if (max <= min) {
    return false;
  }

  value = std::clamp(value, min, max);
  node->manager()->SetValue(*node, base::NumberToString(value));
  return true;
}

jboolean WebContentsAccessibilityAndroid::AreInlineTextBoxesLoaded(
    JNIEnv* env,
    jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  return node->AreInlineTextBoxesLoaded();
}

void WebContentsAccessibilityAndroid::LoadInlineTextBoxes(JNIEnv* env,
                                                          jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (node) {
    node->manager()->LoadInlineTextBoxes(*node);
    RecordInlineTextBoxMetrics(/*from_focus=*/false);
  }
}

ScopedJavaLocalRef<jintArray>
WebContentsAccessibilityAndroid::GetCharacterBoundingBoxes(JNIEnv* env,
                                                           jint unique_id,
                                                           jint start,
                                                           jint len) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return nullptr;
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return nullptr;
  }

  if (len <= 0 || len > kMaxCharacterBoundingBoxLen) {
    LOG(ERROR) << "Trying to request EXTRA_DATA_TEXT_CHARACTER_LOCATION_KEY "
               << "with a length of " << len << ". Valid values are between 1 "
               << "and " << kMaxCharacterBoundingBoxLen;
    return nullptr;
  }

  float dip_scale = 1 / root_manager->device_scale_factor();

  gfx::Rect object_bounds = node->GetUnclippedRootFrameBoundsRect();
  base::FixedArray<int> coords(4 * len);
  for (int i = 0; i < len; i++) {
    gfx::Rect char_bounds = node->GetUnclippedRootFrameInnerTextRangeBoundsRect(
        start + i, start + i + 1);
    if (char_bounds.IsEmpty()) {
      char_bounds = object_bounds;
    }

    char_bounds = gfx::ScaleToEnclosingRect(char_bounds, dip_scale, dip_scale);

    coords[4 * i + 0] = char_bounds.x();
    coords[4 * i + 1] = char_bounds.y();
    coords[4 * i + 2] = char_bounds.right();
    coords[4 * i + 3] = char_bounds.bottom();
  }
  return ToJavaIntArray(env, coords);
}

jboolean WebContentsAccessibilityAndroid::GetImageData(
    JNIEnv* env,
    const JavaParamRef<jobject>& info,
    jint unique_id,
    jboolean has_sent_previous_request) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return false;
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return false;
  }

  // If this is a valid node, try to get an image data url for it.
  std::string image_data_url =
      node->GetStringAttribute(ax::mojom::StringAttribute::kImageDataUrl);
  std::string mimetype;
  std::string charset;
  std::string image_data;
  bool success = net::DataURL::Parse(GURL(image_data_url), &mimetype, &charset,
                                     &image_data);

  if (!success) {
    // If getting the image data url was not successful, then request that the
    // information be added to the node asynchronously (if it has not previously
    // been requested), and return false.
    if (!has_sent_previous_request) {
      root_manager->GetImageData(*node, kMaxImageSize);
    }

    return false;
  }

  ScopedJavaLocalRef<jobject> obj = java_anib_ref_.get(env);
  if (obj.is_null()) {
    return false;
  }

  // If the image data has been retrieved from the image data url successfully,
  // then convert it to a Java byte array and add it in the Bundle extras of
  // the node, and return true.
  Java_AccessibilityNodeInfoBuilder_setAccessibilityNodeInfoImageData(
      env, obj, info, base::android::ToJavaByteArray(env, image_data));

  return true;
}

jint WebContentsAccessibilityAndroid::GetPaintOrder(JNIEnv* env,
                                                    jint unique_id) {
  BrowserAccessibilityManagerAndroid* root_manager =
      GetRootBrowserAccessibilityManager();
  if (!root_manager) {
    return static_cast<jint>(0);
  }

  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return static_cast<jint>(0);
  }

  return static_cast<jint>(node->GetPaintOrder());
}

void WebContentsAccessibilityAndroid::RequestLayoutBasedActions(
    JNIEnv* env,
    jint unique_id,
    const JavaParamRef<jobject>& info) {
  ui::BrowserAccessibility* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return;
  }

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kRequestLayoutBasedAction;
  action_data.target_node_id = node->GetId();
  node->AccessibilityPerformAction(action_data);
}

BrowserAccessibilityManagerAndroid*
WebContentsAccessibilityAndroid::GetRootBrowserAccessibilityManager() const {
  if (snapshot_root_manager_) {
    return snapshot_root_manager_.get();
  }

  return static_cast<BrowserAccessibilityManagerAndroid*>(
      web_contents_->GetRootBrowserAccessibilityManager());
}

BrowserAccessibilityAndroid* WebContentsAccessibilityAndroid::GetAXFromUniqueID(
    int32_t unique_id) const {
  return BrowserAccessibilityAndroid::GetFromUniqueId(unique_id);
}

bool WebContentsAccessibilityAndroid::IsAccessibilityFocused(
    BrowserAccessibilityAndroid* node) const {
  if (!node) {
    return false;
  }
  if (auto* manager = GetRootBrowserAccessibilityManager()) {
    if (auto* focus = static_cast<BrowserAccessibilityAndroid*>(
            manager->GetAccessibilityFocus())) {
      return focus == node;
    }
  }
  return false;
}

void WebContentsAccessibilityAndroid::UpdateFrameInfo(float page_scale) {
  page_scale_ = page_scale;
  if (frame_info_initialized_) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }

  Java_WebContentsAccessibilityImpl_notifyFrameInfoInitialized(env, obj);
  frame_info_initialized_ = true;
}

void WebContentsAccessibilityAndroid::RequestAccessibilityTreeSnapshot(
    JNIEnv* env,
    const JavaParamRef<jobject>& view_structure_root,
    const JavaParamRef<jobject>& accessibility_coordinates,
    const JavaParamRef<jobject>& view,
    const JavaParamRef<jobject>& on_done_callback) {
  // This method should only be called by the unified snapshots feature.
  CHECK(base::FeatureList::IsEnabled(features::kAccessibilityUnifiedSnapshots));

  // This is the callback provided by the Java-side code and will be called
  // after the snapshot has been requested and fully processed. This is not to
  // be confused with the ProcessCompletedAccessibilityTreeSnapshot callback
  // below, which is called once the renderer has returned all AXTreeUpdates.
  on_done_callback_ = std::move(on_done_callback);
  accessibility_coordinates_ = accessibility_coordinates;
  view_ = view;

  base::android::ScopedJavaGlobalRef<jobject> movable_view_structure_root;
  movable_view_structure_root.Reset(env, view_structure_root);

  // Define snapshot parameters:
  auto params = mojom::SnapshotAccessibilityTreeParams::New();
  params->ax_mode = ui::AXMode(ui::kAXModeComplete.flags() | ui::AXMode::kHTML |
                               ui::AXMode::kHTMLMetadata)
                        .flags();
  params->max_nodes = 5000;
  params->timeout = base::Seconds(2);

  // Use AccessibilityTreeSnapshotCombiner to perform snapshots
  auto combiner = base::MakeRefCounted<AccessibilityTreeSnapshotCombiner>(
      base::BindOnce(&WebContentsAccessibilityAndroid::
                         ProcessCompletedAccessibilityTreeSnapshot,
                     GetWeakPtr(), env, std::move(movable_view_structure_root)),
      std::move(params));
  web_contents_->GetPrimaryMainFrame()->ForEachRenderFrameHostImpl(
      [&combiner](RenderFrameHostImpl* rfhi) {
        combiner->RequestSnapshotOnRenderFrameHost(rfhi);
      });
}

void WebContentsAccessibilityAndroid::ProcessCompletedAccessibilityTreeSnapshot(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& view_structure_root,
    ui::AXTreeUpdate& result) {
  // If we don't have a connection back to the Java-side objects, then stop. It
  // may be that the Java-side object was destroyed (e.g. tab closed) before the
  // snapshot was able to finish.
  ScopedJavaLocalRef<jobject> obj = java_adb_ref_.get(env);
  if (!obj) {
    on_done_callback_ = nullptr;
    accessibility_coordinates_ = nullptr;
    view_ = nullptr;
    return;
  }

  // Construct a root manager without a delegate using the snapshot result.
  snapshot_root_manager_ = std::make_unique<BrowserAccessibilityManagerAndroid>(
      result, GetWeakPtr(), *this, /* delegate= */ nullptr);

  auto* root = static_cast<BrowserAccessibilityAndroid*>(
      snapshot_root_manager_->GetBrowserAccessibilityRoot());
  CHECK(root);

  // Construct the Java-side tree, use the JNI builder `java_adb_ref_` to
  // recursively construct each node of the tree starting with the provided
  // root.
  RecursivelyPopulateViewStructureTree(env, obj, root, view_structure_root,
                                       /* is_root= */ true);

  // Add tree-level (root only) data to Java-side tree (e.g. HTML metadata).
  const auto& metadata_strings =
      GetRootBrowserAccessibilityManager()->GetMetadataForTree();
  if (metadata_strings.has_value() && !metadata_strings->empty()) {
    Java_AssistDataBuilder_populateHTMLMetadataProperties(
        env, obj, view_structure_root,
        base::android::ToJavaArrayOfStrings(env, *metadata_strings));
  }

  // We have fulfilled the request for an accessibility tree snapshot, so we can
  // now call the provided Java-side callback to inform original client that the
  // async construction is complete.
  base::android::RunRunnableAndroid(on_done_callback_);
}

void WebContentsAccessibilityAndroid::RecursivelyPopulateViewStructureTree(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> obj,
    const BrowserAccessibilityAndroid* node,
    const base::android::JavaRef<jobject>& java_side_assist_data_object,
    bool is_root) {
  PopulateViewStructureNode(env, obj, node, java_side_assist_data_object);
  for (size_t child_index = 0; const auto& child : node->PlatformChildren()) {
    const auto& child_node =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    ScopedJavaLocalRef<jobject> java_side_child_object =
        Java_AssistDataBuilder_addChildNode(
            env, obj, java_side_assist_data_object, child_index);
    child_index++;
    RecursivelyPopulateViewStructureTree(env, obj, &child_node,
                                         java_side_child_object,
                                         /* is_root= */ false);
  }
  if (!is_root) {
    Java_AssistDataBuilder_commitNode(env, obj, java_side_assist_data_object);
  }
}

void WebContentsAccessibilityAndroid::PopulateViewStructureNode(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> obj,
    const BrowserAccessibilityAndroid* node,
    const base::android::JavaRef<jobject>& java_side_assist_data_object) {
  Java_AssistDataBuilder_populateBaseProperties(
      env, obj, java_side_assist_data_object,
      GetCanonicalJNIString(env, node->GetClassName()), node->GetChildCount());

  int bgcolor = 0;
  int color = 0;
  int text_size = -1.0;
  if (node->HasFloatAttribute(ax::mojom::FloatAttribute::kFontSize)) {
    color = node->GetIntAttribute(ax::mojom::IntAttribute::kColor);
    bgcolor = node->GetIntAttribute(ax::mojom::IntAttribute::kBackgroundColor);
    text_size = node->GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize);
  }
  Java_AssistDataBuilder_populateTextProperties(
      env, obj, java_side_assist_data_object,
      base::android::ConvertUTF16ToJavaString(env, node->GetTextContentUTF16()),
      node->GetSelectedItemCount() > 0, node->GetSelectionStart(),
      node->GetSelectionEnd(), color, bgcolor, text_size,
      node->HasTextStyle(ax::mojom::TextStyle::kBold),
      node->HasTextStyle(ax::mojom::TextStyle::kItalic),
      node->HasTextStyle(ax::mojom::TextStyle::kUnderline),
      node->HasTextStyle(ax::mojom::TextStyle::kLineThrough));

  float dip_scale =
      1 /
      web_contents_->GetPrimaryMainFrame()->AccessibilityGetDeviceScaleFactor();
  gfx::Rect absolute_rect = gfx::ScaleToEnclosingRect(
      node->GetUnclippedRootFrameBoundsRect(), dip_scale, dip_scale);

  Java_AssistDataBuilder_populateBoundsProperties(
      env, obj, java_side_assist_data_object, absolute_rect.x(),
      absolute_rect.y(), absolute_rect.width(), absolute_rect.height(),
      accessibility_coordinates_, view_);

  std::vector<std::vector<std::u16string>> html_attrs;
  for (const auto& attr : node->GetHtmlAttributes()) {
    html_attrs.push_back(
        {base::UTF8ToUTF16(attr.first), base::UTF8ToUTF16(attr.second)});
  }
  Java_AssistDataBuilder_populateHTMLProperties(
      env, obj, java_side_assist_data_object,
      GetCanonicalJNIString(
          env, node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag)),
      GetCanonicalJNIString(
          env, node->GetStringAttribute(ax::mojom::StringAttribute::kDisplay)),
      base::android::ToJavaArrayOfStringArray(env, html_attrs));
}

ScopedJavaLocalRef<jobject>
WebContentsAccessibilityAndroid::ToJavaCanonicalStringRangesMap(
    JNIEnv* env,
    const std::optional<absl::flat_hash_map<std::string, RangePairs>>&
        text_style_map,
    int* ranges_count) {
  return ToJavaRangesMap(
      env, text_style_map,
      &Java_AccessibilityNodeInfoUtils_setTextAttributeRangesMapStringValue,
      base::BindRepeating(
          [](WebContentsAccessibilityAndroid& node, JNIEnv* env,
             const std::string& value) {
            return node.GetCanonicalJNIString(env, value);
          },
          std::ref(*this), base::Unretained(env)),
      ranges_count);
}

base::WeakPtr<WebContentsAccessibilityAndroid>
WebContentsAccessibilityAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ScopedJavaLocalRef<jintArray>
WebContentsAccessibilityAndroid::GetChildIdsForTesting(JNIEnv* env,
                                                       jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return nullptr;
  }
  std::vector<int> child_ids;
  for (const auto& child : node->PlatformChildren()) {
    const auto& android_node =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    child_ids.push_back(android_node.GetUniqueId());
  }
  return base::android::ToJavaIntArray(env, child_ids);
}

ScopedJavaLocalRef<jintArray>
WebContentsAccessibilityAndroid::GetLabeledByNodeIdsForTesting(JNIEnv* env,
                                                               jint unique_id) {
  BrowserAccessibilityAndroid* node = GetAXFromUniqueID(unique_id);
  if (!node) {
    return nullptr;
  }
  return base::android::ToJavaIntArray(env, node->GetLabelledByAndroidIds());
}

static jlong JNI_WebContentsAccessibilityImpl_InitWithAXTree(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong ax_tree_update_ptr,
    const JavaParamRef<jobject>& jaccessibility_node_info_builder) {
  return reinterpret_cast<intptr_t>(new WebContentsAccessibilityAndroid(
      env, obj, ax_tree_update_ptr, jaccessibility_node_info_builder));
}

static jlong JNI_WebContentsAccessibilityImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jaccessibility_node_info_builder) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  return reinterpret_cast<intptr_t>(new WebContentsAccessibilityAndroid(
      env, obj, web_contents, jaccessibility_node_info_builder));
}

static jlong JNI_WebContentsAccessibilityImpl_InitForAssistData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jassist_data_builder) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  return reinterpret_cast<intptr_t>(new WebContentsAccessibilityAndroid(
      env, obj, jassist_data_builder, web_contents));
}

}  // namespace content

DEFINE_JNI(AccessibilityNodeInfoBuilder)
DEFINE_JNI(AccessibilityNodeInfoUtils)
DEFINE_JNI(AssistDataBuilder)
DEFINE_JNI(WebContentsAccessibilityImpl)
