// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/annotated_page_content_extraction_utils.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "components/autofill/core/common/unique_ids.h"
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
constexpr char kAnchorDataKey[] = "anchorData";
constexpr char kUrlKey[] = "url";
constexpr char kRelKey[] = "rel";
constexpr char kImageInfoKey[] = "imageInfo";
constexpr char kImageCaptionKey[] = "imageCaption";
constexpr char kAnnotatedRolesKey[] = "annotatedRoles";
constexpr char kIframeDataKey[] = "iframeData";
constexpr char kTableRowDataKey[] = "tableRowData";
constexpr char kRowTypeKey[] = "rowType";
constexpr char kCanvasDataKey[] = "canvasData";
constexpr char kVideoDataKey[] = "videoData";
constexpr char kLayoutSizeKey[] = "layoutSize";
constexpr char kWidthKey[] = "width";
constexpr char kHeightKey[] = "height";
constexpr char kFrameTokenKey[] = "frameToken";
constexpr char kTokenValueKey[] = "value";
constexpr char kContentKey[] = "content";
constexpr char kLocalFrameDataKey[] = "localFrameData";
constexpr char kSourceURLKey[] = "sourceUrl";
constexpr char kTitleKey[] = "title";
constexpr char kChildrenNodesKey[] = "childrenNodes";
constexpr char kDomNodeIdKey[] = "domNodeId";
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

// Populates `destination_frame_data` from the `local_frame_data` content.
void PopulateFrameData(
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

  const base::DictValue* interaction_info_dict =
      local_frame_data.FindDict(kFrameInteractionInfoKey);
  if (interaction_info_dict) {
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
}

// Populates the iframe data of the `destination_node` from the
// `iframe_data` content.
void PopulateIframeData(
    const base::DictValue& iframe_data,
    optimization_guide::proto::ContentNode* destination_node,
    const url::Origin& origin) {
  if (const base::DictValue* content = iframe_data.FindDict(kContentKey)) {
    if (const base::DictValue* local_frame_data =
            content->FindDict(kLocalFrameDataKey)) {
      optimization_guide::proto::FrameData* node_frame_data =
          destination_node->mutable_content_attributes()
              ->mutable_iframe_data()
              ->mutable_frame_data();
      PopulateFrameData(*local_frame_data, node_frame_data, origin);
    }
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

// Populates the form control data of the `destination_node` from the
// `form_control_data` content.
void PopulateFormControlData(
    const base::DictValue& form_control_data,
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

}  // namespace

void PopulateAPCNodeFromContentTree(
    const base::DictValue& node_content,
    const url::Origin& origin,
    FrameGrafter& grafter,
    optimization_guide::proto::ContentNode* destination_node) {
  if (!destination_node) {
    return;
  }

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
        if (const base::DictValue* frame_token =
                iframe_data->FindDict(kFrameTokenKey)) {
          if (const std::string* token_string =
                  frame_token->FindString(kTokenValueKey)) {
            // If we have a remote token, it means the content is in another
            // frame (likely cross-origin) and we should register a placeholder.
            // We do not populate children or other data in this case.
            if (!token_string->empty()) {
              if (std::optional<autofill::RemoteFrameToken> remote =
                      DeserializeFrameIdAsRemoteFrameToken(*token_string)) {
                grafter.RegisterPlaceholder(*remote, destination_node);
              }
              return;
            }
          }
        }
        PopulateIframeData(*iframe_data, destination_node, origin);
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
        PopulateFormControlData(*form_control_data, destination_node);
      }
      break;
    }
    default:
      break;
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

  // Recursively populate children.
  if (const base::ListValue* children_nodes =
          node_content.FindList(kChildrenNodesKey)) {
    for (const base::Value& child_value : *children_nodes) {
      if (child_value.is_dict()) {
        PopulateAPCNodeFromContentTree(child_value.GetDict(), origin, grafter,
                                       destination_node->add_children_nodes());
      }
    }
  }

  return;
}

void PopulateFrameDataNode(
    const base::DictValue& frame_data_content,
    const url::Origin& origin,
    optimization_guide::proto::FrameData* destination_frame_data_node) {
  CHECK(destination_frame_data_node);
  PopulateFrameData(frame_data_content, destination_frame_data_node, origin);
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
