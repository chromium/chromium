// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/annotated_page_content_extraction_utils.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/string_number_conversions.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/core/page_content_proto_serializer.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/frame_grafter.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/web/public/web_state.h"

// TODO(crbug.com/464473686): Measure interesting error cases.

namespace {

// APC JSON Keys Used in Extraction.
constexpr char kAttributeTypeKey[] = "attributeType";
constexpr char kContentAttributesKey[] = "contentAttributes";
constexpr char kTextInfoKey[] = "textInfo";
constexpr char kTextContentKey[] = "textContent";
constexpr char kTextStyleKey[] = "textStyle";
constexpr char kHasEmphasisKey[] = "hasEmphasis";
constexpr char kTextSizeKey[] = "textSize";
constexpr char kColorKey[] = "color";
constexpr char kAnchorDataKey[] = "anchorData";
constexpr char kUrlKey[] = "url";
constexpr char kRelKey[] = "rel";
constexpr char kImageInfoKey[] = "imageInfo";
constexpr char kImageCaptionKey[] = "imageCaption";
constexpr char kAnnotatedRolesKey[] = "annotatedRoles";
constexpr char kIframeDataKey[] = "iframeData";
constexpr char kTableDataKey[] = "tableData";
constexpr char kTableNameKey[] = "tableName";
constexpr char kTableRowDataKey[] = "tableRowData";
constexpr char kRowTypeKey[] = "rowType";
constexpr char kCanvasDataKey[] = "canvasData";
constexpr char kVideoDataKey[] = "videoData";
constexpr char kAriaRoleKey[] = "ariaRole";
constexpr char kLayoutSizeKey[] = "layoutSize";
constexpr char kWidthKey[] = "width";
constexpr char kHeightKey[] = "height";
constexpr char kRemoteFrameTokenKey[] = "remoteFrameToken";
constexpr char kLocalFrameTokenKey[] = "localFrameToken";
constexpr char kTokenValueKey[] = "value";
constexpr char kContentKey[] = "content";
constexpr char kLocalFrameDataKey[] = "localFrameData";
constexpr char kSourceURLKey[] = "sourceUrl";
constexpr char kTitleKey[] = "title";
constexpr char kContainsPaidContentKey[] = "containsPaidContent";
constexpr char kChildrenNodesKey[] = "childrenNodes";
constexpr char kDomNodeIdKey[] = "domNodeId";
constexpr char kAutofillNodeIdKey[] = "autofillNodeId";
constexpr char kLabelForDomNodeIdKey[] = "labelForDomNodeId";
constexpr char kFrameInteractionInfoKey[] = "frameInteractionInfo";
constexpr char kSelectionKey[] = "selection";
constexpr char kStartDomNodeIdKey[] = "startDomNodeId";
constexpr char kStartOffsetKey[] = "startOffset";
constexpr char kEndDomNodeIdKey[] = "endDomNodeId";
constexpr char kEndOffsetKey[] = "endOffset";
constexpr char kSelectedTextKey[] = "selectedText";
constexpr char kFocusedDomNodeIdKey[] = "focusedDomNodeId";
constexpr char kDocumentIdKey[] = "documentId";
constexpr char kFormControlDataKey[] = "formControlData";
constexpr char kFormControlTypeKey[] = "formControlType";
constexpr char kFieldNameKey[] = "fieldName";
constexpr char kFieldValueKey[] = "fieldValue";
constexpr char kSelectOptionsKey[] = "selectOptions";
constexpr char kOptionValueKey[] = "value";
constexpr char kOptionTextKey[] = "text";
constexpr char kIsSelectedKey[] = "isSelected";
constexpr char kIsDisabledKey[] = "disabled";
constexpr char kPlaceholderKey[] = "placeholder";
constexpr char kIsCheckedKey[] = "isChecked";
constexpr char kIsRequiredKey[] = "isRequired";
constexpr char kIsReadonlyKey[] = "isReadonly";
constexpr char kRedactionDecisionKey[] = "redactionDecision";
constexpr char kFormDataKey[] = "formData";
constexpr char kFormNameKey[] = "formName";
constexpr char kFormActionUrlKey[] = "actionUrl";
constexpr char kNodeInteractionInfoKey[] = "nodeInteractionInfo";
constexpr char kScrollerInfoKey[] = "scrollerInfo";
constexpr char kScrollingBoundsKey[] = "scrollingBounds";
constexpr char kVisibleAreaKey[] = "visibleArea";
constexpr char kUserScrollableHorizontalKey[] = "userScrollableHorizontal";
constexpr char kUserScrollableVerticalKey[] = "userScrollableVertical";
constexpr char kXKey[] = "x";
constexpr char kYKey[] = "y";
constexpr char kIsFocusableKey[] = "isFocusable";
constexpr char kClickabilityReasonsKey[] = "clickabilityReasons";
constexpr char kInteractionDisabledReasonsKey[] = "interactionDisabledReasons";
constexpr char kIsDisabledInteractionKey[] = "isDisabled";
constexpr char kDocumentScopedZOrderKey[] = "documentScopedZOrder";
constexpr char kMediaDataKey[] = "mediaData";
constexpr char kMediaDataTypeKey[] = "mediaDataType";
constexpr char kDurationMillisecondsKey[] = "durationMilliseconds";
constexpr char kCurrentPositionMillisecondsKey[] =
    "currentPositionMilliseconds";
constexpr char kIsPlayingKey[] = "isPlaying";
constexpr char kLabelKey[] = "label";
constexpr char kGeometryKey[] = "geometry";
constexpr char kOuterBoundingBoxKey[] = "outerBoundingBox";
constexpr char kVisibleBoundingBoxKey[] = "visibleBoundingBox";
constexpr char kFragmentVisibleBoundingBoxesKey[] =
    "fragmentVisibleBoundingBoxes";
constexpr char kIsFocusedDocumentKey[] = "isFocusedDocument";

// Reads a JS number (double) from a `dict` stored under `key`.
std::optional<int> ReadJsNumber(const base::DictValue& dict, const char* key) {
  if (std::optional<double> value = dict.FindDouble(key)) {
    return static_cast<int>(*value);
  }
  return std::nullopt;
}

// Reads a JS number (double) from a `value`.
std::optional<int> ReadJsNumber(const base::Value& value) {
  if (std::optional<double> double_value = value.GetIfDouble()) {
    return static_cast<int>(*double_value);
  }
  return std::nullopt;
}

// Reads a token value from the `iframe_data` dictionary under the specified
// `key`. Returns an optional string that is only set if the token is found and
// is not empty.
std::optional<std::string> GetTokenValueFromIframeData(
    const base::DictValue& iframe_data,
    std::string_view key) {
  if (const base::DictValue* token_dict = iframe_data.FindDict(key)) {
    if (const std::string* token_string =
            token_dict->FindString(kTokenValueKey)) {
      return *token_string;
    }
  }
  return std::nullopt;
}

// Populates the text data of a content `destination_node` from the `text_data`
// content.
void PopulateTextData(
    const base::DictValue& text_data,
    optimization_guide::proto::ContentNode* destination_node) {
  if (const std::string* text_content = text_data.FindString(kTextContentKey)) {
    destination_node->mutable_content_attributes()
        ->mutable_text_data()
        ->set_text_content(*text_content);
  }

  // Handle Emphasis and Text Size.
  if (const base::DictValue* text_style = text_data.FindDict(kTextStyleKey)) {
    std::optional<bool> has_emphasis = text_style->FindBool(kHasEmphasisKey);
    if (has_emphasis) {
      destination_node->mutable_content_attributes()
          ->mutable_text_data()
          ->mutable_text_style()
          ->set_has_emphasis(*has_emphasis);
    }
    std::optional<int> text_size = ReadJsNumber(*text_style, kTextSizeKey);
    if (text_size && optimization_guide::proto::TextSize_IsValid(*text_size)) {
      destination_node->mutable_content_attributes()
          ->mutable_text_data()
          ->mutable_text_style()
          ->set_text_size(
              static_cast<optimization_guide::proto::TextSize>(*text_size));
    }

    if (const std::string* color_str = text_style->FindString(kColorKey)) {
      uint32_t color_val = 0;
      if (base::StringToUint(*color_str, &color_val)) {
        destination_node->mutable_content_attributes()
            ->mutable_text_data()
            ->mutable_text_style()
            ->set_color(color_val);
      }
    }
  }
}

// Populates the anchor data of the `destination_node` from the
// `anchor_data` content.
void PopulateAnchorData(
    const base::DictValue& anchor_data,
    optimization_guide::proto::ContentNode* destination_node) {
  if (const std::string* url = anchor_data.FindString(kUrlKey)) {
    destination_node->mutable_content_attributes()
        ->mutable_anchor_data()
        ->set_url(*url);
  }
  if (const base::ListValue* rels = anchor_data.FindList(kRelKey)) {
    for (const base::Value& rel : *rels) {
      std::optional<int> rel_value = ReadJsNumber(rel);
      if (rel_value &&
          optimization_guide::proto::AnchorRel_IsValid(*rel_value)) {
        destination_node->mutable_content_attributes()
            ->mutable_anchor_data()
            ->add_rel(
                static_cast<optimization_guide::proto::AnchorRel>(*rel_value));
      }
    }
  }
}

// Populates the image data of the `destination_node` from the
// `image_data` content.
void PopulateImageData(
    const base::DictValue& image_data,
    optimization_guide::proto::ContentNode* destination_node) {
  if (const std::string* image_caption =
          image_data.FindString(kImageCaptionKey)) {
    destination_node->mutable_content_attributes()
        ->mutable_image_data()
        ->set_image_caption(*image_caption);
  }
}

// Populates the canvas data of the `destination_node` from the
// `canvas_data` content.
void PopulateCanvasData(
    const base::DictValue& canvas_data,
    optimization_guide::proto::ContentNode* destination_node) {
  if (const base::DictValue* layout_size =
          canvas_data.FindDict(kLayoutSizeKey)) {
    if (std::optional<int> width = ReadJsNumber(*layout_size, kWidthKey)) {
      destination_node->mutable_content_attributes()
          ->mutable_canvas_data()
          ->set_layout_width(*width);
    }
    if (std::optional<int> height = ReadJsNumber(*layout_size, kHeightKey)) {
      destination_node->mutable_content_attributes()
          ->mutable_canvas_data()
          ->set_layout_height(*height);
    }
  }
}

// Populates the video data of the `destination_node` from the
// `video_data` content.
void PopulateVideoData(
    const base::DictValue& video_data,
    optimization_guide::proto::ContentNode* destination_node) {
  if (const std::string* url = video_data.FindString(kUrlKey)) {
    destination_node->mutable_content_attributes()
        ->mutable_video_data()
        ->set_url(*url);
  }
}

// Populates `destination_frame_data`'s media data from the `media_data`
// content.
void PopulateMediaData(
    const base::DictValue& media_data,
    optimization_guide::proto::FrameData* destination_frame_data) {
  optimization_guide::proto::MediaData* media_data_proto =
      destination_frame_data->mutable_media_data();

  if (std::optional<int> media_type =
          ReadJsNumber(media_data, kMediaDataTypeKey)) {
    if (optimization_guide::proto::MediaDataType_IsValid(*media_type)) {
      media_data_proto->set_media_data_type(
          static_cast<optimization_guide::proto::MediaDataType>(*media_type));
    }
  }

  if (std::optional<int> duration =
          ReadJsNumber(media_data, kDurationMillisecondsKey)) {
    media_data_proto->set_duration_milliseconds(*duration);
  }

  if (std::optional<int> position =
          ReadJsNumber(media_data, kCurrentPositionMillisecondsKey)) {
    media_data_proto->set_current_position_milliseconds(*position);
  }

  media_data_proto->set_is_playing(
      media_data.FindBool(kIsPlayingKey).value_or(false));
}

// Populates the given `destination_frame_data` from the `local_frame_data`
// JSON dictionary content representing the frame data extracted from the
// renderer. Returns a FrameDataNodeResult containing the focus state.
FrameDataNodeResult PopulateFrameData(
    const base::DictValue& local_frame_data,
    optimization_guide::proto::FrameData* destination_frame_data,
    const url::Origin& origin) {
  // Always populate the security origin.
  optimization_guide::SecurityOriginSerializer::Serialize(
      origin, destination_frame_data->mutable_security_origin());

  if (const std::string* url_ptr = local_frame_data.FindString(kSourceURLKey)) {
    GURL url(*url_ptr);
    if (url.is_valid() && url.SchemeIs(url::kDataScheme)) {
      destination_frame_data->set_url("data:");
    } else {
      destination_frame_data->set_url(*url_ptr);
    }
  }

  if (const std::string* title_ptr = local_frame_data.FindString(kTitleKey)) {
    destination_frame_data->set_title(*title_ptr);
  }

  std::optional<bool> contains_paid_content =
      local_frame_data.FindBool(kContainsPaidContentKey);
  if (contains_paid_content && *contains_paid_content) {
    destination_frame_data->mutable_paid_content_metadata()
        ->set_contains_paid_content(true);
  }

  const base::DictValue* interaction_info_dict =
      local_frame_data.FindDict(kFrameInteractionInfoKey);
  if (interaction_info_dict) {
    if (std::optional<int> focused_dom_node_id =
            ReadJsNumber(*interaction_info_dict, kFocusedDomNodeIdKey)) {
      destination_frame_data->mutable_frame_interaction_info()
          ->set_focused_node_id(*focused_dom_node_id);
    }

    const base::DictValue* selection_dict =
        interaction_info_dict->FindDict(kSelectionKey);
    if (selection_dict) {
      optimization_guide::proto::Selection* selection =
          destination_frame_data->mutable_frame_interaction_info()
              ->mutable_selection();

      if (std::optional<int> start_node_id =
              ReadJsNumber(*selection_dict, kStartDomNodeIdKey)) {
        selection->set_start_node_id(*start_node_id);
      }

      if (std::optional<int> start_offset =
              ReadJsNumber(*selection_dict, kStartOffsetKey)) {
        selection->set_start_offset(*start_offset);
      }

      if (std::optional<int> end_node_id =
              ReadJsNumber(*selection_dict, kEndDomNodeIdKey)) {
        selection->set_end_node_id(*end_node_id);
      }

      if (std::optional<int> end_offset =
              ReadJsNumber(*selection_dict, kEndOffsetKey)) {
        selection->set_end_offset(*end_offset);
      }

      if (const std::string* selected_text =
              selection_dict->FindString(kSelectedTextKey)) {
        selection->set_selected_text(*selected_text);
      }
    }
  }

  const std::string* document_id_ptr =
      local_frame_data.FindString(kDocumentIdKey);
  if (document_id_ptr && !document_id_ptr->empty()) {
    destination_frame_data->mutable_document_identifier()->set_serialized_token(
        *document_id_ptr);
  }

  if (const base::DictValue* media_data =
          local_frame_data.FindDict(kMediaDataKey)) {
    PopulateMediaData(*media_data, destination_frame_data);
  }

  return {.is_focused =
              local_frame_data.FindBool(kIsFocusedDocumentKey).value_or(false)};
}

// Populates the iframe data of the `destination_node` from the
// `iframe_data` content. Calls `on_frame_extracted` with the extracted frame.
void PopulateIframeData(
    const base::DictValue& iframe_data,
    optimization_guide::proto::ContentNode* destination_node,
    const url::Origin& origin,
    const base::RepeatingCallback<void(bool is_focused,
                                       const std::string& document_id)>&
        on_frame_extracted) {
  if (const base::DictValue* content = iframe_data.FindDict(kContentKey)) {
    if (const base::DictValue* local_frame_data =
            content->FindDict(kLocalFrameDataKey)) {
      optimization_guide::proto::FrameData* node_frame_data =
          destination_node->mutable_content_attributes()
              ->mutable_iframe_data()
              ->mutable_frame_data();
      FrameDataNodeResult result =
          PopulateFrameData(*local_frame_data, node_frame_data, origin);
      if (on_frame_extracted) {
        on_frame_extracted.Run(
            result.is_focused,
            node_frame_data->document_identifier().serialized_token());
      }
    }
  }
}

// Populates the table data of the `destination_node` from the
// `table_data` content.
void PopulateTableData(
    const base::DictValue& table_data,
    optimization_guide::proto::ContentNode* destination_node) {
  if (const std::string* table_name = table_data.FindString(kTableNameKey)) {
    destination_node->mutable_content_attributes()
        ->mutable_table_data()
        ->set_table_name(*table_name);
  }
}

// Populates the table row data of the `destination_node` from the
// `table_row_data` content.
void PopulateTableRowData(
    const base::DictValue& table_row_data,
    optimization_guide::proto::ContentNode* destination_node) {
  if (std::optional<int> row_type = ReadJsNumber(table_row_data, kRowTypeKey)) {
    if (optimization_guide::proto::TableRowType_IsValid(*row_type)) {
      destination_node->mutable_content_attributes()
          ->mutable_table_row_data()
          ->set_type(
              static_cast<optimization_guide::proto::TableRowType>(*row_type));
    }
  }
}

// Populates the Autofill data associated with the `form_control_data`.
void PopulateAutofillData(
    const base::DictValue& form_control_data,
    AutofillExtractionContext* autofill_context,
    optimization_guide::proto::FormControlData* proto_form_control_data) {
  if (!autofill_context) {
    return;
  }

  std::optional<int> autofill_node_id =
      ReadJsNumber(form_control_data, kAutofillNodeIdKey);
  if (!autofill_node_id) {
    return;
  }

  std::optional<AutofillFieldMetadata> autofill_metadata =
      GetAutofillFieldData(*autofill_node_id, *autofill_context);
  if (!autofill_metadata) {
    return;
  }

  proto_form_control_data->set_autofill_section_id(
      autofill_metadata->section_id);
  proto_form_control_data->add_coarse_autofill_field_type(
      autofill_metadata->coarse_field_type);

  if (!autofill_context->extract_autofill_credit_card_redactions) {
    return;
  }

  // Prioritize existing redaction decisions (e.g., from JS) over Autofill.
  if (proto_form_control_data->redaction_decision() !=
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY) {
    return;
  }

  optimization_guide::proto::RedactionDecision autofill_redaction_decision =
      ConvertAutofillFieldRedactionReason(*proto_form_control_data,
                                          autofill_metadata->redaction_reason);

  if (autofill_redaction_decision ==
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY) {
    return;
  }

  proto_form_control_data->set_redaction_decision(autofill_redaction_decision);

  if (ShouldRedactContent(proto_form_control_data->redaction_decision(),
                          *autofill_context)) {
    proto_form_control_data->clear_field_value();
  }
}

// Populates the form control data of the `destination_node` from the
// `form_control_data` content. `autofill_context` is optional and, if provided,
// used to populate Autofill form data and apply redaction.
void PopulateFormControlData(
    const base::DictValue& form_control_data,
    AutofillExtractionContext* autofill_context,
    optimization_guide::proto::ContentNode* destination_node) {
  optimization_guide::proto::FormControlData* proto_form_control_data =
      destination_node->mutable_content_attributes()
          ->mutable_form_control_data();

  if (std::optional<int> form_control_type =
          ReadJsNumber(form_control_data, kFormControlTypeKey)) {
    if (optimization_guide::proto::FormControlType_IsValid(
            *form_control_type)) {
      proto_form_control_data->set_form_control_type(
          static_cast<optimization_guide::proto::FormControlType>(
              *form_control_type));
    }
  }

  if (const std::string* field_name =
          form_control_data.FindString(kFieldNameKey)) {
    proto_form_control_data->set_field_name(*field_name);
  }

  if (const std::string* field_value =
          form_control_data.FindString(kFieldValueKey)) {
    proto_form_control_data->set_field_value(*field_value);
  }

  if (const std::string* placeholder =
          form_control_data.FindString(kPlaceholderKey)) {
    proto_form_control_data->set_placeholder(*placeholder);
  }

  proto_form_control_data->set_is_checked(
      form_control_data.FindBool(kIsCheckedKey).value_or(false));

  proto_form_control_data->set_is_required(
      form_control_data.FindBool(kIsRequiredKey).value_or(false));

  if (form_control_data.FindBool(kIsReadonlyKey).value_or(false)) {
    // Temporarily map readonly to disabled. This is a lossy workaround that
    // preserves "do not edit" intent for consumers that only read proto data.
    // TODO(crbug.com/481361478): Add readonly field to FormControlData proto.
    destination_node->mutable_content_attributes()
        ->mutable_interaction_info()
        ->set_is_disabled(true);
  }

  if (std::optional<int> redaction_decision =
          ReadJsNumber(form_control_data, kRedactionDecisionKey)) {
    if (optimization_guide::proto::RedactionDecision_IsValid(
            *redaction_decision)) {
      proto_form_control_data->set_redaction_decision(
          static_cast<optimization_guide::proto::RedactionDecision>(
              *redaction_decision));
    }
  }

  if (const base::ListValue* select_options =
          form_control_data.FindList(kSelectOptionsKey)) {
    for (const base::Value& option_value : *select_options) {
      if (option_value.is_dict()) {
        const base::DictValue* option_dict = &option_value.GetDict();
        optimization_guide::proto::SelectOption* proto_select_option =
            proto_form_control_data->add_select_options();

        if (const std::string* value =
                option_dict->FindString(kOptionValueKey)) {
          proto_select_option->set_value(*value);
        }
        if (const std::string* text = option_dict->FindString(kOptionTextKey)) {
          proto_select_option->set_text(*text);
        }
        proto_select_option->set_is_selected(
            option_dict->FindBool(kIsSelectedKey).value_or(false));
        proto_select_option->set_is_disabled(
            option_dict->FindBool(kIsDisabledKey).value_or(false));
      }
    }
  }

  PopulateAutofillData(form_control_data, autofill_context,
                       proto_form_control_data);
}

// Populates the form data of the `destination_node` from the `form_data`
// content.
void PopulateFormInfo(
    const base::DictValue& form_data,
    optimization_guide::proto::ContentNode* destination_node) {
  optimization_guide::proto::FormInfo* proto_form_info =
      destination_node->mutable_content_attributes()->mutable_form_data();

  if (const std::string* form_name = form_data.FindString(kFormNameKey)) {
    proto_form_info->set_form_name(*form_name);
  }

  if (const std::string* action_url = form_data.FindString(kFormActionUrlKey)) {
    proto_form_info->set_action_url(*action_url);
  }
}

// Populates the `destination_node` with the `node_interaction_data` content.
void PopulateNodeInteractionInfo(
    const base::DictValue& node_interaction_data,
    optimization_guide::proto::ContentNode* destination_node) {
  auto* info_proto = destination_node->mutable_content_attributes()
                         ->mutable_interaction_info();

  // Scroller Info.
  if (const base::DictValue* scroller_info =
          node_interaction_data.FindDict(kScrollerInfoKey)) {
    auto* scroller_proto = info_proto->mutable_scroller_info();
    if (const base::DictValue* bounds =
            scroller_info->FindDict(kScrollingBoundsKey)) {
      if (std::optional<int> width = ReadJsNumber(*bounds, kWidthKey)) {
        scroller_proto->mutable_scrolling_bounds()->set_width(*width);
      }
      if (std::optional<int> height = ReadJsNumber(*bounds, kHeightKey)) {
        scroller_proto->mutable_scrolling_bounds()->set_height(*height);
      }
    }
    if (const base::DictValue* visible_area =
            scroller_info->FindDict(kVisibleAreaKey)) {
      auto* rect = scroller_proto->mutable_visible_area();
      if (std::optional<int> x = ReadJsNumber(*visible_area, kXKey)) {
        rect->set_x(*x);
      }
      if (std::optional<int> y = ReadJsNumber(*visible_area, kYKey)) {
        rect->set_y(*y);
      }
      if (std::optional<int> width = ReadJsNumber(*visible_area, kWidthKey)) {
        rect->set_width(*width);
      }
      if (std::optional<int> height = ReadJsNumber(*visible_area, kHeightKey)) {
        rect->set_height(*height);
      }
    }
    scroller_proto->set_user_scrollable_horizontal(
        scroller_info->FindBool(kUserScrollableHorizontalKey).value_or(false));
    scroller_proto->set_user_scrollable_vertical(
        scroller_info->FindBool(kUserScrollableVerticalKey).value_or(false));
  }

  // Focusable.
  info_proto->set_is_focusable(
      node_interaction_data.FindBool(kIsFocusableKey).value_or(false));

  // Clickability Reasons.
  if (const base::ListValue* reasons =
          node_interaction_data.FindList(kClickabilityReasonsKey)) {
    for (const auto& reason : *reasons) {
      if (std::optional<int> r = ReadJsNumber(reason)) {
        if (optimization_guide::proto::ClickabilityReason_IsValid(*r)) {
          info_proto->add_clickability_reasons(
              static_cast<optimization_guide::proto::ClickabilityReason>(*r));
        }
      }
    }
  }

  // Disabled.
  info_proto->set_is_disabled(
      node_interaction_data.FindBool(kIsDisabledInteractionKey)
          .value_or(false));

  // Disabled Reasons.
  if (const base::ListValue* disabled_reasons =
          node_interaction_data.FindList(kInteractionDisabledReasonsKey)) {
    for (const auto& reason : *disabled_reasons) {
      if (std::optional<int> r = ReadJsNumber(reason)) {
        if (optimization_guide::proto::InteractionDisabledReason_IsValid(*r)) {
          info_proto->add_interaction_disabled_reasons(
              static_cast<optimization_guide::proto::InteractionDisabledReason>(
                  *r));
        }
      }
    }
  }

  // Document Scoped Z-Order.
  if (std::optional<int> z_order =
          ReadJsNumber(node_interaction_data, kDocumentScopedZOrderKey)) {
    info_proto->set_document_scoped_z_order(*z_order);
  }
}

// Extracts and returns a BoundingRect proto from the given dictionary.
// Returns std::nullopt if any required dimension is missing or invalid,
// ensuring we do not set potentially misleading 0 values.
std::optional<optimization_guide::proto::BoundingRect> ExtractBoundingRect(
    const base::DictValue& dict) {
  std::optional<int> x = ReadJsNumber(dict, kXKey);
  std::optional<int> y = ReadJsNumber(dict, kYKey);
  std::optional<int> width = ReadJsNumber(dict, kWidthKey);
  std::optional<int> height = ReadJsNumber(dict, kHeightKey);

  if (!x || !y || !width || !height) {
    // Do not create the rectangle if one part is missing.
    return std::nullopt;
  }

  optimization_guide::proto::BoundingRect proto;
  proto.set_x(*x);
  proto.set_y(*y);
  proto.set_width(*width);
  proto.set_height(*height);
  return proto;
}

// Populates the geometry data of the `node` from the `geometry_dict` content.
// Extracts outer bounding box, visible bounding box, and any fragments if
// applicable.
void PopulateGeometry(const base::DictValue& geometry_dict,
                      optimization_guide::proto::ContentNode* node) {
  auto mutable_geometry = [node]() {
    return node->mutable_content_attributes()->mutable_geometry();
  };

  if (const base::DictValue* outer_box =
          geometry_dict.FindDict(kOuterBoundingBoxKey)) {
    if (std::optional<optimization_guide::proto::BoundingRect> box =
            ExtractBoundingRect(*outer_box)) {
      *mutable_geometry()->mutable_outer_bounding_box() = std::move(*box);
    }
  }

  if (const base::DictValue* visible_box =
          geometry_dict.FindDict(kVisibleBoundingBoxKey)) {
    if (std::optional<optimization_guide::proto::BoundingRect> box =
            ExtractBoundingRect(*visible_box)) {
      *mutable_geometry()->mutable_visible_bounding_box() = std::move(*box);
    }
  }

  if (const base::ListValue* fragments =
          geometry_dict.FindList(kFragmentVisibleBoundingBoxesKey)) {
    for (const auto& fragment : *fragments) {
      if (const base::DictValue* fragment_dict = fragment.GetIfDict()) {
        // Add the fragment to the list of fragments.
        if (std::optional<optimization_guide::proto::BoundingRect> box =
                ExtractBoundingRect(*fragment_dict)) {
          *mutable_geometry()->add_fragment_visible_bounding_boxes() =
              std::move(*box);
        }
      }
    }
  }
}
}  // namespace

void PopulateAPCNodeFromContentTree(
    const base::DictValue& node_content,
    const url::Origin& origin,
    FrameGrafter& grafter,
    AutofillExtractionContext* autofill_context,
    optimization_guide::proto::ContentNode* destination_node,
    const base::RepeatingCallback<void(bool is_focused,
                                       const std::string& document_id)>&
        on_frame_extracted) {
  if (!destination_node) {
    return;
  }

  std::optional<AutofillExtractionContext> child_autofill_context;

  const base::DictValue* content_attributes =
      node_content.FindDict(kContentAttributesKey);

  // A node must have content attribute to be populated.
  if (!content_attributes) {
    return;
  }

  // Handle DOM Node ID.
  if (std::optional<int> dom_node_id =
          ReadJsNumber(*content_attributes, kDomNodeIdKey)) {
    destination_node->mutable_content_attributes()
        ->set_common_ancestor_dom_node_id(*dom_node_id);
  }

  if (std::optional<int> label_for_id =
          ReadJsNumber(*content_attributes, kLabelForDomNodeIdKey)) {
    destination_node->mutable_content_attributes()->set_label_for_dom_node_id(
        *label_for_id);
  }

  // Populate the attribute type.
  std::optional<optimization_guide::proto::ContentAttributeType> type;

  std::optional<int> attribute_type =
      ReadJsNumber(*content_attributes, kAttributeTypeKey);
  if (attribute_type && optimization_guide::proto::ContentAttributeType_IsValid(
                            *attribute_type)) {
    type = static_cast<optimization_guide::proto::ContentAttributeType>(
        *attribute_type);
    destination_node->mutable_content_attributes()->set_attribute_type(*type);
  } else {
    // TODO(crbug.com/464473686): Record a histogram for this error case.
    // A node must have a valid attribute type to be populated.
    return;
  }

  switch (*type) {
    case optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT: {
      const base::DictValue* text_data =
          content_attributes->FindDict(kTextInfoKey);
      if (text_data) {
        PopulateTextData(*text_data, destination_node);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR: {
      const base::DictValue* anchor_data =
          content_attributes->FindDict(kAnchorDataKey);
      if (anchor_data) {
        PopulateAnchorData(*anchor_data, destination_node);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE: {
      const base::DictValue* image_data =
          content_attributes->FindDict(kImageInfoKey);
      if (image_data) {
        PopulateImageData(*image_data, destination_node);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_CANVAS: {
      if (const base::DictValue* canvas_data =
              content_attributes->FindDict(kCanvasDataKey)) {
        PopulateCanvasData(*canvas_data, destination_node);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_VIDEO: {
      if (const base::DictValue* video_data =
              content_attributes->FindDict(kVideoDataKey)) {
        PopulateVideoData(*video_data, destination_node);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME: {
      if (const base::DictValue* iframe_data =
              content_attributes->FindDict(kIframeDataKey)) {
        // TODO(crbug.com/464473686): Record anomaly if there is a remote token
        // and children for an iframe node (which has content attribute of type
        // iframe and a number of children nodes > 0); those 2 should be
        // mutally exclusive.
        if (std::optional<std::string> token_string =
                GetTokenValueFromIframeData(*iframe_data,
                                            kRemoteFrameTokenKey)) {
          // If we have a remote token, it means the content is in another
          // frame (likely cross-origin) and we should register a placeholder.
          // We do not populate children or other data in this case which will
          // be populated later on via the frame grafter.
          if (std::optional<autofill::RemoteFrameToken> remote =
                  DeserializeFrameIdAsRemoteFrameToken(*token_string)) {
            grafter.RegisterPlaceholder(*remote, destination_node);
            return;
          }
        }

        if (autofill_context) {
          if (std::optional<std::string> token_string =
                  GetTokenValueFromIframeData(*iframe_data,
                                              kLocalFrameTokenKey)) {
            if (std::optional<autofill::LocalFrameToken> local_token =
                    DeserializeFrameIdAsLocalFrameToken(*token_string)) {
              child_autofill_context.emplace(
                  autofill_context->web_state, local_token,
                  autofill_context->extract_autofill_credit_card_redactions,
                  autofill_context->section_numbers);
            }
          }
        }

        // We do not skip populating the proto data for the frame even if we
        // failed to extract the child context (e.g. missing local token).
        // This matches Blink's behavior where availability of content is
        // prioritized over strict guarantee of redaction when Autofill data
        // is missing while required.
        PopulateIframeData(*iframe_data, destination_node, origin,
                           on_frame_extracted);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE: {
      const base::DictValue* table_data =
          content_attributes->FindDict(kTableDataKey);
      if (table_data) {
        PopulateTableData(*table_data, destination_node);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE_ROW: {
      const base::DictValue* table_row_data =
          content_attributes->FindDict(kTableRowDataKey);
      if (table_row_data) {
        PopulateTableRowData(*table_row_data, destination_node);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_FORM: {
      const base::DictValue* form_data =
          content_attributes->FindDict(kFormDataKey);
      if (form_data) {
        PopulateFormInfo(*form_data, destination_node);
      }
      break;
    }
    case optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL: {
      const base::DictValue* form_control_data =
          content_attributes->FindDict(kFormControlDataKey);
      if (form_control_data) {
        PopulateFormControlData(*form_control_data, autofill_context,
                                destination_node);
      }
      break;
    }
    default:
      break;
  }

  // Handle Interaction Info.
  const base::DictValue* interaction_info =
      content_attributes->FindDict(kNodeInteractionInfoKey);
  if (interaction_info) {
    PopulateNodeInteractionInfo(*interaction_info, destination_node);
  }

  // Handle ARIA Label.
  if (const std::string* label = content_attributes->FindString(kLabelKey)) {
    destination_node->mutable_content_attributes()->set_label(*label);
  }

  // Handle Geometry.
  if (const base::DictValue* geometry =
          content_attributes->FindDict(kGeometryKey)) {
    PopulateGeometry(*geometry, destination_node);
  }

  // Handle Annotated Role.
  if (const base::ListValue* annotated_roles =
          content_attributes->FindList(kAnnotatedRolesKey)) {
    for (const base::Value& role : *annotated_roles) {
      std::optional<int> role_value = ReadJsNumber(role);
      if (role_value &&
          optimization_guide::proto::AnnotatedRole_IsValid(*role_value)) {
        destination_node->mutable_content_attributes()->add_annotated_roles(
            static_cast<optimization_guide::proto::AnnotatedRole>(*role_value));
      }
    }
  }

  // Handle ARIA Role.
  if (std::optional<int> aria_role =
          ReadJsNumber(*content_attributes, kAriaRoleKey)) {
    if (optimization_guide::proto::AXRole_IsValid(*aria_role)) {
      destination_node->mutable_content_attributes()->set_aria_role(
          static_cast<optimization_guide::proto::AXRole>(*aria_role));
    }
  }

  // Recursively populate children.
  if (const base::ListValue* children_nodes =
          node_content.FindList(kChildrenNodesKey)) {
    for (const base::Value& child_value : *children_nodes) {
      if (child_value.is_dict()) {
        PopulateAPCNodeFromContentTree(
            child_value.GetDict(), origin, grafter,
            child_autofill_context ? &*child_autofill_context
                                   : autofill_context,
            destination_node->add_children_nodes(), on_frame_extracted);
      }
    }
  }
}

FrameDataNodeResult PopulateFrameDataNode(
    const base::DictValue& frame_data_content,
    const url::Origin& origin,
    optimization_guide::proto::FrameData* destination_frame_data_node) {
  CHECK(destination_frame_data_node);
  return PopulateFrameData(frame_data_content, destination_frame_data_node,
                           origin);
}

// Populates `page_interaction_info_node` from the
// `page_interaction_info_content` content.
void PopulatePageInteractionInfoNode(
    const base::DictValue& page_interaction_info_content,
    optimization_guide::proto::PageInteractionInfo*
        destination_page_interaction_info_node) {
  CHECK_EQ(destination_page_interaction_info_node->ByteSizeLong(), 0u);
  if (std::optional<int> focused_node_id =
          ReadJsNumber(page_interaction_info_content, kFocusedDomNodeIdKey)) {
    destination_page_interaction_info_node->set_focused_node_id(
        *focused_node_id);
  }
}

void PopulateViewportGeometryNode(
    const base::DictValue& viewport_geometry_content,
    optimization_guide::proto::BoundingRect*
        destination_viewport_geometry_node) {
  // Check that the destination node is only populated once.
  CHECK_EQ(destination_viewport_geometry_node->ByteSizeLong(), 0u);

  if (std::optional<int> x = ReadJsNumber(viewport_geometry_content, kXKey)) {
    destination_viewport_geometry_node->set_x(*x);
  }
  if (std::optional<int> y = ReadJsNumber(viewport_geometry_content, kYKey)) {
    destination_viewport_geometry_node->set_y(*y);
  }
  if (std::optional<int> width =
          ReadJsNumber(viewport_geometry_content, kWidthKey)) {
    destination_viewport_geometry_node->set_width(*width);
  }
  if (std::optional<int> height =
          ReadJsNumber(viewport_geometry_content, kHeightKey)) {
    destination_viewport_geometry_node->set_height(*height);
  }
}

void PopulateAutofillInformation(
    web::WebState* web_state,
    optimization_guide::proto::AutofillInformation* autofill_information) {
  autofill::AutofillClientIOS* client =
      autofill::AutofillClientIOS::FromWebState(web_state);
  if (!client || !client->HasPersonalDataManager()) {
    return;
  }

  const autofill::PersonalDataManager& pdm = client->GetPersonalDataManager();

  bool address_autofill_enabled = client->IsAutofillProfileEnabled();
  bool has_address_profiles = !pdm.address_data_manager().GetProfiles().empty();
  if (address_autofill_enabled && has_address_profiles) {
    autofill_information->add_fillable_data(
        optimization_guide::proto::AutofillInformation_FillableData_ADDRESS);
  }

  bool payment_autofill_enabled = false;
  bool has_credit_cards = false;
  if (auto* payments_client = client->GetPaymentsAutofillClient()) {
    payment_autofill_enabled =
        payments_client->IsAutofillPaymentMethodsEnabled();
    has_credit_cards = !pdm.payments_data_manager().GetCreditCards().empty();
  }
  if (payment_autofill_enabled && has_credit_cards) {
    autofill_information->add_fillable_data(
        optimization_guide::proto::
            AutofillInformation_FillableData_CREDIT_CARD);
  }
}

void ResolveCrossSiteFrameContent(
    FrameGrafter& grafter,
    autofill::ChildFrameRegistrar* registrar,
    optimization_guide::proto::AnnotatedPageContent* apc) {
  CHECK(registrar);
  auto mapping_lookup = base::BindRepeating(
      [](autofill::ChildFrameRegistrar* registrar,
         autofill::RemoteFrameToken remote) {
        return registrar->LookupChildFrame(remote);
      },
      registrar);

  auto placer = base::BindRepeating(
      [](optimization_guide::proto::ContentNode* parentNode,
         FrameGrafter::FrameContent unregistered) {
        *parentNode->add_children_nodes() = std::move(unregistered.content);
      },
      apc->mutable_root_node());
  grafter.ResolveUnregisteredContent(mapping_lookup, placer);
}

void ResolveFocusedFrame(
    std::vector<FrameFocusInfo>& focused_frame_infos,
    const std::vector<autofill::RemoteFrameToken>& remote_frames,
    autofill::ChildFrameRegistrar* registrar,
    optimization_guide::proto::AnnotatedPageContent* apc) {
  CHECK(registrar);
  for (const autofill::RemoteFrameToken& remote_token : remote_frames) {
    if (std::optional<autofill::LocalFrameToken> local_token =
            registrar->LookupChildFrame(remote_token)) {
      for (auto& info : focused_frame_infos) {
        if (info.local_token && *info.local_token == *local_token) {
          info.document_id = remote_token.ToString();
        }
      }
    }
  }

  // Pick the first frame that is focused and has a document id as the
  // focused_frame.
  for (const auto& info : focused_frame_infos) {
    if (!info.document_id.empty()) {
      apc->mutable_page_interaction_info()
          ->mutable_focused_frame()
          ->set_serialized_token(info.document_id);
      break;
    }
  }
}
