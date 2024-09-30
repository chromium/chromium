/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/context_menu_controller.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-blink.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editing_tri_state.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/context_menu_provider.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/bindings/script_regexp.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

constexpr char kPasswordRe[] =
    // Synonyms and abbreviations of password.
    "pass(?:word|code)|pas(?:word|code)|pswrd|psw|pswd|pwd|parole|watchword|"

    // Translations.
    "pasahitza|parol|lozinka|sifr|contrasenya|heslo|adgangskode|losen|"
    "wachtwoord|paswoord|salasana|passe|contrasinal|passwort|jelszo|"
    "sandi|signum|slaptazodis|kata|passord|haslo|senha|geslo|contrasena|"
    "khau";

void SetPasswordManagerData(Element* element, ContextMenuData& data) {
  // Uses heuristics (finding 'password' and its short versions and translations
  // in field name and id etc.) to recognize a field intended for password input
  // of plain text HTML field type or `HasBeenPasswordField` which returns true
  // due to either server predictions or user's masking of input values. It is
  // used to set the field is_password_type_by_heuristics.
  if (auto* input = DynamicTo<HTMLInputElement>(element)) {
    const AtomicString& id = input->GetIdAttribute();
    const AtomicString& name = input->GetNameAttribute();

    auto* isolate = element->GetDocument().GetAgent().isolate();
    auto* per_isolate_data = V8PerIsolateData::From(isolate);
    if (!per_isolate_data->GetPasswordRegexp()) {
      per_isolate_data->SetPasswordRegexp(MakeGarbageCollected<ScriptRegexp>(
          element->GetDocument().GetAgent().isolate(), kPasswordRe,
          kTextCaseUnicodeInsensitive));
    }

    auto* password_regexp = per_isolate_data->GetPasswordRegexp();
    data.is_password_type_by_heuristics =
        (data.form_control_type == mojom::blink::FormControlType::kInputText ||
         data.form_control_type == mojom::blink::FormControlType::kInputEmail ||
         data.form_control_type ==
             mojom::blink::FormControlType::kInputSearch ||
         data.form_control_type == mojom::blink::FormControlType::kInputUrl ||
         data.form_control_type == mojom::blink::FormControlType::kTextArea) &&
        (password_regexp->Match(id.GetString()) >= 0 ||
         password_regexp->Match(name.GetString()) >= 0 ||
         input->HasBeenPasswordField());
  }
}

void SetAutofillData(Node* node, ContextMenuData& data) {
  if (auto* form_control = DynamicTo<HTMLFormControlElement>(node)) {
    data.form_control_type = form_control->FormControlType();
    data.field_renderer_id = form_control->GetDomNodeId();
    if (auto* form = form_control->Form()) {
      data.form_renderer_id = form->GetDomNodeId();
    } else {
      data.form_renderer_id = 0;
    }
  }
  if (auto* html_element =
          node ? DynamicTo<HTMLElement>(RootEditableElement(*node)) : nullptr) {
    ContentEditableType content_editable =
        html_element->contentEditableNormalized();
    data.is_content_editable_for_autofill =
        (content_editable == ContentEditableType::kPlaintextOnly ||
         content_editable == ContentEditableType::kContentEditable) &&
        !DynamicTo<HTMLFormElement>(node) &&
        !DynamicTo<HTMLFormControlElement>(node);
    if (data.is_content_editable_for_autofill) {
      data.field_renderer_id = html_element->GetDomNodeId();
      data.form_renderer_id = html_element->GetDomNodeId();
    }
  }
}

// Returns true if node or any of its ancestors have a context menu event
// listener. Uses already_visited_nodes to track nodes which have already
// been checked across multiple calls to this function, which could cause
// the output to be false despite having an ancestor context menu listener.
bool UnvisitedNodeOrAncestorHasContextMenuListener(
    Node* node,
    HeapHashSet<Member<Node>>& already_visited_nodes) {
  Node* current_node_for_parent_traversal = node;
  while (current_node_for_parent_traversal != nullptr) {
    if (current_node_for_parent_traversal->HasEventListeners(
            event_type_names::kContextmenu)) {
      return true;
    }
    // If we've already checked this node, all of its ancestors must not
    // have had listeners (or, we already detected a listener and broke out
    // early).
    if (!already_visited_nodes.insert(current_node_for_parent_traversal)
             .is_new_entry) {
      break;
    }
    current_node_for_parent_traversal =
        current_node_for_parent_traversal->parentNode();
  }
  return false;
}

template <class enumType>
uint32_t EnumToBitmask(enumType outcome) {
  return 1 << static_cast<uint8_t>(outcome);
}

}  // namespace

ContextMenuController::ContextMenuController(Page* page) : page_(page) {}

ContextMenuController::~ContextMenuController() = default;

void ContextMenuController::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(menu_provider_);
  visitor->Trace(hit_test_result_);
  visitor->Trace(context_menu_client_receiver_);
  visitor->Trace(image_selection_cached_result_);
}

void ContextMenuController::ClearContextMenu() {
  if (menu_provider_)
    menu_provider_->ContextMenuCleared();
  menu_provider_ = nullptr;
  context_menu_client_receiver_.reset();
  hit_test_result_ = HitTestResult();
  image_selection_cached_result_ = nullptr;
}

void ContextMenuController::DocumentDetached(Document* document) {
  if (Node* inner_node = hit_test_result_.InnerNode()) {
    // Invalidate the context menu info if its target document is detached.
    if (inner_node->GetDocument() == document)
      ClearContextMenu();
  }
}

void ContextMenuController::HandleContextMenuEvent(MouseEvent* mouse_event) {
  DCHECK(mouse_event->type() == event_type_names::kContextmenu);
  LocalFrame* frame = mouse_event->target()->ToNode()->GetDocument().GetFrame();
  PhysicalOffset location =
      PhysicalOffset::FromPointFRound(mouse_event->AbsoluteLocation());

  if (ShowContextMenu(frame, location, mouse_event->GetMenuSourceType(),
                      mouse_event))
    mouse_event->SetDefaultHandled();
}

void ContextMenuController::ShowContextMenuAtPoint(
    LocalFrame* frame,
    float x,
    float y,
    ContextMenuProvider* menu_provider) {
  menu_provider_ = menu_provider;
  if (!ShowContextMenu(frame, PhysicalOffset(LayoutUnit(x), LayoutUnit(y)),
                       kMenuSourceNone))
    ClearContextMenu();
}

void ContextMenuController::CustomContextMenuItemSelected(unsigned action) {
  if (!menu_provider_)
    return;
  menu_provider_->ContextMenuItemSelected(action);
  ClearContextMenu();
}

Node* ContextMenuController::GetContextMenuNodeWithImageContents() {
  uint32_t outcome = 0;
  uint32_t hit_test_depth = 0;
  LocalFrame* top_hit_frame =
      hit_test_result_.InnerNode()->GetDocument().GetFrame();
  Node* found_image_node = nullptr;
  HeapHashSet<Member<Node>> already_visited_nodes_for_context_menu_listener;

  for (const auto& raw_node : hit_test_result_.ListBasedTestResult()) {
    hit_test_depth++;
    Node* node = raw_node.Get();

    // Execute context menu listener and cross frame checks before image check
    // because these checks should also apply to the image node itself before
    // breaking.
    if (UnvisitedNodeOrAncestorHasContextMenuListener(
            node, already_visited_nodes_for_context_menu_listener)) {
      outcome |=
          EnumToBitmask(ImageSelectionOutcome::kFoundContextMenuListener);
      // Don't break because it allows us to log the failure reason only
      // if an image node was otherwise available lower in the hit test.
    }
    if (top_hit_frame != node->GetDocument().GetFrame()) {
      outcome |= EnumToBitmask(ImageSelectionOutcome::kBlockedByCrossFrameNode);
      // Don't break because it allows us to log the failure reason only
      // if an image node was otherwise available lower in the hit test.
    }

    if (IsA<HTMLCanvasElement>(node) ||
        !HitTestResult::AbsoluteImageURL(node).IsEmpty()) {
      found_image_node = node;

      if (hit_test_depth == 1) {
        outcome |= EnumToBitmask(ImageSelectionOutcome::kImageFoundStandard);
        // The context menu listener check is only necessary when penetrating,
        // so clear the bit so we don't want to log it if the image was on top.
        outcome &=
            ~EnumToBitmask(ImageSelectionOutcome::kFoundContextMenuListener);
      } else {
        outcome |= EnumToBitmask(ImageSelectionOutcome::kImageFoundPenetrating);
      }
      break;
    }
    // IMPORTANT: Check after image checks above so that non-transparent
    // image elements don't trigger the opaque check.
    if (node->GetLayoutBox() != nullptr &&
        node->GetLayoutBox()->BackgroundIsKnownToBeOpaqueInRect(
            HitTestLocation::RectForPoint(
                hit_test_result_.PointInInnerNodeFrame()))) {
      outcome |= EnumToBitmask(ImageSelectionOutcome::kBlockedByOpaqueNode);
      // Don't break because it allows us to log the failure reason only
      // if an image node was otherwise available lower in the hit test.
    }
  }

  // Only log if we found an image node within the hit test.
  if (found_image_node != nullptr) {
    base::UmaHistogramCounts1000("Blink.ContextMenu.ImageSelection.Depth",
                                 hit_test_depth);
    for (uint32_t i = 0;
         i <= static_cast<uint8_t>(ImageSelectionOutcome::kMaxValue); i++) {
      unsigned val = 1 << i;
      if (outcome & val) {
        base::UmaHistogramEnumeration(
            "Blink.ContextMenu.ImageSelection.Outcome",
            ImageSelectionOutcome(i));
      }
    }
  }
  // If there is anything preventing this image selection, return nullptr.
  uint32_t blocking_image_selection_mask =
      ~(EnumToBitmask(ImageSelectionOutcome::kImageFoundStandard) |
        EnumToBitmask(ImageSelectionOutcome::kImageFoundPenetrating));
  if (outcome & blocking_image_selection_mask) {
    return nullptr;
  }
  image_selection_cached_result_ = found_image_node;
  return found_image_node;
}

Node* ContextMenuController::ContextMenuImageNodeForFrame(LocalFrame* frame) {
  ImageSelectionRetrievalOutcome outcome;
  // We currently will fail to retrieve an image if another hit test is made
  // on a non-image node is made before retrieval of the image.
  if (!image_selection_cached_result_) {
    outcome = ImageSelectionRetrievalOutcome::kImageNotFound;
  } else if (image_selection_cached_result_->GetDocument().GetFrame() !=
             frame) {
    outcome = ImageSelectionRetrievalOutcome::kCrossFrameRetrieval;
  } else {
    outcome = ImageSelectionRetrievalOutcome::kImageFound;
  }

  base::UmaHistogramEnumeration(
      "Blink.ContextMenu.ImageSelection.RetrievalOutcome", outcome);

  return outcome == ImageSelectionRetrievalOutcome::kImageFound
             ? image_selection_cached_result_
             : nullptr;
}

// TODO(crbug.com/1184297) Cache image node when the context menu is shown and
//    return that rather than refetching.
Node* ContextMenuController::ContextMenuNodeForFrame(LocalFrame* frame) {
  return hit_test_result_.InnerNodeFrame() == frame
             ? hit_test_result_.InnerNodeOrImageMapImage()
             : nullptr;
}

void ContextMenuController::CustomContextMenuAction(uint32_t action) {
  CustomContextMenuItemSelected(action);
}

void ContextMenuController::ContextMenuClosed(const KURL& link_followed) {
  if (link_followed.IsValid()) {
    WebLocalFrameImpl* selected_web_frame =
        WebLocalFrameImpl::FromFrame(hit_test_result_.InnerNodeFrame());
    if (selected_web_frame)
      selected_web_frame->SendPings(link_followed);
  }
  ClearContextMenu();
}

static int ComputeEditFlags(Document& selected_document, Editor& editor) {
  int edit_flags = ContextMenuDataEditFlags::kCanDoNone;
  if (editor.CanUndo())
    edit_flags |= ContextMenuDataEditFlags::kCanUndo;
  if (editor.CanRedo())
    edit_flags |= ContextMenuDataEditFlags::kCanRedo;
  if (editor.CanCut())
    edit_flags |= ContextMenuDataEditFlags::kCanCut;
  if (editor.CanCopy())
    edit_flags |= ContextMenuDataEditFlags::kCanCopy;
  if (editor.CanPaste())
    edit_flags |= ContextMenuDataEditFlags::kCanPaste;
  if (editor.CanDelete())
    edit_flags |= ContextMenuDataEditFlags::kCanDelete;
  if (editor.CanEditRichly())
    edit_flags |= ContextMenuDataEditFlags::kCanEditRichly;
  if (IsA<HTMLDocument>(selected_document) ||
      selected_document.IsXHTMLDocument()) {
    edit_flags |= ContextMenuDataEditFlags::kCanTranslate;
    if (selected_document.queryCommandEnabled("selectAll", ASSERT_NO_EXCEPTION))
      edit_flags |= ContextMenuDataEditFlags::kCanSelectAll;
  }
  return edit_flags;
}

static gfx::Rect ComputeSelectionRect(LocalFrame* selected_frame) {
  gfx::Rect anchor;
  gfx::Rect focus;
  selected_frame->Selection().ComputeAbsoluteBounds(anchor, focus);
  anchor = selected_frame->View()->ConvertToRootFrame(anchor);
  focus = selected_frame->View()->ConvertToRootFrame(focus);

  gfx::Rect combined_rect = anchor;
  combined_rect.UnionEvenIfEmpty(focus);

  // Intersect the selection rect and the visible bounds of the focused_element
  // to ensure the selection rect is visible.
  Document* doc = selected_frame->GetDocument();
  if (doc) {
    if (Element* focused_element = doc->FocusedElement())
      combined_rect.Intersect(focused_element->VisibleBoundsInLocalRoot());
  }

  // TODO(bokan): This method may not work as expected when the local root
  // isn't the main frame since the result won't be transformed and clipped by
  // the visual viewport (which is accessible only from the outermost main
  // frame).
  if (selected_frame->LocalFrameRoot().IsOutermostMainFrame()) {
    VisualViewport& visual_viewport =
        selected_frame->GetPage()->GetVisualViewport();

    gfx::Rect rect_in_visual_viewport =
        visual_viewport.RootFrameToViewport(combined_rect);
    rect_in_visual_viewport.Intersect(gfx::Rect(visual_viewport.Size()));
    return rect_in_visual_viewport;
  }

  return combined_rect;
}

bool ContextMenuController::ShouldShowContextMenuFromTouch(
    const ContextMenuData& data) {
  return page_->GetSettings().GetAlwaysShowContextMenuOnTouch() ||
         !data.link_url.is_empty() ||
         data.media_type == mojom::blink::ContextMenuDataMediaType::kImage ||
         data.media_type == mojom::blink::ContextMenuDataMediaType::kVideo ||
         data.is_editable || data.opened_from_highlight ||
         !data.selected_text.empty();
}

bool ContextMenuController::ShowContextMenu(LocalFrame* frame,
                                            const PhysicalOffset& point,
                                            WebMenuSourceType source_type,
                                            const MouseEvent* mouse_event) {
  // Displaying the context menu in this function is a big hack as we don't
  // have context, i.e. whether this is being invoked via a script or in
  // response to user input (Mouse event WM_RBUTTONDOWN,
  // Keyboard events KeyVK_APPS, Shift+F10). Check if this is being invoked
  // in response to the above input events before popping up the context menu.
  if (!ContextMenuAllowedScope::IsContextMenuAllowed())
    return false;

  if (context_menu_client_receiver_.is_bound())
    context_menu_client_receiver_.reset();

  HitTestRequest::HitTestRequestType type =
      HitTestRequest::kReadOnly | HitTestRequest::kActive |
      HitTestRequest::kPenetratingList | HitTestRequest::kListBased;

  HitTestLocation location(point);
  HitTestResult result(type, location);
  if (frame)
    result = frame->GetEventHandler().HitTestResultAtLocation(location, type);
  if (!result.InnerNodeOrImageMapImage())
    return false;

  // Clear any previously set cached results if we are resetting the hit test
  // result.
  image_selection_cached_result_ = nullptr;

  hit_test_result_ = result;
  result.SetToShadowHostIfInUAShadowRoot();

  LocalFrame* selected_frame = result.InnerNodeFrame();
  // Tests that do not require selection pass mouse_event = nullptr
  if (mouse_event) {
    selected_frame->GetEventHandler()
        .GetSelectionController()
        .UpdateSelectionForContextMenuEvent(
            mouse_event, hit_test_result_,
            PhysicalOffset(ToFlooredPoint(point)));
  }

  ContextMenuData data;
  data.mouse_position = selected_frame->View()->FrameToViewport(
      result.RoundedPointInInnerNodeFrame());

  data.edit_flags = ComputeEditFlags(
      *selected_frame->GetDocument(),
      To<LocalFrame>(page_->GetFocusController().FocusedOrMainFrame())
          ->GetEditor());

  if (mouse_event && source_type == kMenuSourceKeyboard) {
    Node* target_node = mouse_event->target()->ToNode();
    if (target_node && IsA<Element>(target_node)) {
      // Get the url from an explicitly set target, e.g. the focused element
      // when the context menu is evoked from the keyboard. Note: the innerNode
      // could also be set. It is used to identify a relevant inner media
      // element. In most cases, the innerNode will already be set to any
      // relevant inner media element via the median x,y point from the focused
      // element's bounding box. As the media element in most cases fills the
      // entire area of a focused link or button, this generally suffices.
      // Example: When Shift+F10 is used with <a><img></a>, any image-related
      // context menu options, such as open image in new tab, must be presented.
      result.SetURLElement(target_node->EnclosingLinkEventParentOrSelf());
    }
  }
  data.link_url = GURL(result.AbsoluteLinkURL());

  auto* html_element = DynamicTo<HTMLElement>(result.InnerNode());
  if (html_element) {
    data.title_text = html_element->title().Utf8();
    data.alt_text = html_element->AltText().Utf8();
  }
  if (!result.AbsoluteMediaURL().IsEmpty() ||
      result.GetMediaStreamDescriptor() || result.GetMediaSourceHandle()) {
    if (!result.AbsoluteMediaURL().IsEmpty())
      data.src_url = GURL(result.AbsoluteMediaURL());

    // We know that if absoluteMediaURL() is not empty or element has a media
    // stream descriptor or element has a media source handle, then this is a
    // media element.
    auto* media_element = To<HTMLMediaElement>(result.InnerNode());
    if (IsA<HTMLVideoElement>(*media_element)) {
      // A video element should be presented as an audio element when it has an
      // audio track but no video track.
      if (media_element->HasAudio() && !media_element->HasVideo()) {
        data.media_type = mojom::blink::ContextMenuDataMediaType::kAudio;
      } else {
        data.media_type = mojom::blink::ContextMenuDataMediaType::kVideo;
      }

      if (media_element->SupportsPictureInPicture()) {
        data.media_flags |= ContextMenuData::kMediaCanPictureInPicture;
        if (PictureInPictureController::IsElementInPictureInPicture(
                media_element))
          data.media_flags |= ContextMenuData::kMediaPictureInPicture;
      }

      auto* video_element = To<HTMLVideoElement>(media_element);
      if (video_element->HasReadableVideoFrame()) {
        data.media_flags |= ContextMenuData::kMediaHasReadableVideoFrame;
      }
    } else if (IsA<HTMLAudioElement>(*media_element)) {
      data.media_type = mojom::blink::ContextMenuDataMediaType::kAudio;
    }

    data.suggested_filename = media_element->title().Utf8();
    if (media_element->error())
      data.media_flags |= ContextMenuData::kMediaInError;
    if (media_element->paused())
      data.media_flags |= ContextMenuData::kMediaPaused;
    if (media_element->muted())
      data.media_flags |= ContextMenuData::kMediaMuted;
    if (media_element->SupportsLoop())
      data.media_flags |= ContextMenuData::kMediaCanLoop;
    if (media_element->Loop())
      data.media_flags |= ContextMenuData::kMediaLoop;
    if (media_element->SupportsSave())
      data.media_flags |= ContextMenuData::kMediaCanSave;
    if (media_element->HasAudio())
      data.media_flags |= ContextMenuData::kMediaHasAudio;
    if (media_element->HasVideo()) {
      data.media_flags |= ContextMenuData::kMediaHasVideo;
    }
    if (media_element->IsEncrypted()) {
      data.media_flags |= ContextMenuData::kMediaEncrypted;
    }

    // Media controls can be toggled only for video player. If we toggle
    // controls for audio then the player disappears, and there is no way to
    // return it back. Don't set this bit for fullscreen video, since
    // toggling is ignored in that case.
    if (IsA<HTMLVideoElement>(media_element) && media_element->HasVideo() &&
        !media_element->IsFullscreen())
      data.media_flags |= ContextMenuData::kMediaCanToggleControls;
    if (media_element->ShouldShowAllControls())
      data.media_flags |= ContextMenuData::kMediaControls;
  } else if (IsA<HTMLObjectElement>(*result.InnerNode()) ||
             IsA<HTMLEmbedElement>(*result.InnerNode())) {
    if (auto* embedded = DynamicTo<LayoutEmbeddedContent>(
            result.InnerNode()->GetLayoutObject())) {
      WebPluginContainerImpl* plugin_view = embedded->Plugin();
      if (plugin_view) {
        data.media_type = mojom::blink::ContextMenuDataMediaType::kPlugin;

        WebPlugin* plugin = plugin_view->Plugin();
        data.link_url = GURL(KURL(plugin->LinkAtPosition(data.mouse_position)));

        auto* plugin_element = To<HTMLPlugInElement>(result.InnerNode());
        data.src_url = GURL(
            plugin_element->GetDocument().CompleteURL(plugin_element->Url()));

        // Figure out the text selection and text edit flags.
        WebString text = plugin->SelectionAsText();
        if (!text.IsEmpty()) {
          data.selected_text = text.Utf8();
          if (plugin->CanCopy())
            data.edit_flags |= ContextMenuDataEditFlags::kCanCopy;
        }
        bool plugin_can_edit_text = plugin->CanEditText();
        if (plugin_can_edit_text) {
          data.is_editable = true;
          if (!!(data.edit_flags & ContextMenuDataEditFlags::kCanCopy))
            data.edit_flags |= ContextMenuDataEditFlags::kCanCut;
          data.edit_flags |= ContextMenuDataEditFlags::kCanPaste;

          if (plugin->HasEditableText())
            data.edit_flags |= ContextMenuDataEditFlags::kCanSelectAll;

          if (plugin->CanUndo())
            data.edit_flags |= ContextMenuDataEditFlags::kCanUndo;
          if (plugin->CanRedo())
            data.edit_flags |= ContextMenuDataEditFlags::kCanRedo;
        }
        // Disable translation for plugins.
        data.edit_flags &= ~ContextMenuDataEditFlags::kCanTranslate;

        // Figure out the media flags.
        data.media_flags |= ContextMenuData::kMediaCanSave;
        if (plugin->SupportsPaginatedPrint())
          data.media_flags |= ContextMenuData::kMediaCanPrint;

        // Add context menu commands that are supported by the plugin.
        // Only show rotate view options if focus is not in an editable text
        // area.
        if (!plugin_can_edit_text && plugin->CanRotateView())
          data.media_flags |= ContextMenuData::kMediaCanRotate;
      }
    }
  } else {
    // Check image media last to ensure that penetrating image selection
    // does not override a topmost media element.
    // TODO(benwgold): Consider extending penetration to all media types.
    Node* potential_image_node = result.InnerNodeOrImageMapImage();
    SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
        "Blink.ContextMenu.ImageSelection.ElapsedTime");
    potential_image_node = GetContextMenuNodeWithImageContents();

    if (potential_image_node != nullptr &&
        IsA<HTMLCanvasElement>(potential_image_node)) {
      data.media_type = mojom::blink::ContextMenuDataMediaType::kCanvas;
      // TODO(crbug.com/1267243): Support WebGPU canvas.
      data.has_image_contents =
          !To<HTMLCanvasElement>(potential_image_node)->IsWebGPU();
    } else if (potential_image_node != nullptr &&
               !HitTestResult::AbsoluteImageURL(potential_image_node)
                    .IsEmpty()) {
      data.src_url =
          GURL(HitTestResult::AbsoluteImageURL(potential_image_node));
      data.media_type = mojom::blink::ContextMenuDataMediaType::kImage;
      data.media_flags |= ContextMenuData::kMediaCanPrint;
      data.has_image_contents =
          HitTestResult::GetImage(potential_image_node) &&
          !HitTestResult::GetImage(potential_image_node)->IsNull();
    }
  }
  // If it's not a link, an image, a media element, or an image/media link,
  // show a selection menu or a more generic page menu.
  if (selected_frame->GetDocument()->Loader()) {
    data.frame_encoding =
        selected_frame->GetDocument()->EncodingName().GetString().Utf8();
  }

  data.selection_start_offset = 0;
  // HitTestResult::isSelected() ensures clean layout by performing a hit test.
  // If source_type is |kMenuSourceAdjustSelection| or
  // |kMenuSourceAdjustSelectionReset| we know the original HitTestResult in
  // SelectionController passed the inside check already, so let it pass.
  if (result.IsSelected(location) ||
      source_type == kMenuSourceAdjustSelection ||
      source_type == kMenuSourceAdjustSelectionReset) {
    // Remove any unselectable content from the selected text.
    data.selected_text =
        selected_frame
            ->SelectedText(TextIteratorBehavior::Builder()
                               .SetSkipsUnselectableContent(true)
                               .Build())
            .Utf8();
    WebRange range =
        selected_frame->GetInputMethodController().GetSelectionOffsets();
    // TODO(crbug.com/40093243): `range.StartOffset()` shouldn't be negative but
    // it happens. crbug.com/40093243#comment28 suggested not to show context
    // menu. For now, prefer showing it at wrong place/data than not showing.
    if (range.StartOffset() >= 0) {
      data.selection_start_offset = range.StartOffset();
    }
    if (!result.IsContentEditable()) {
      TextFragmentHandler::OpenedContextMenuOverSelection(selected_frame);
      AnnotationAgentContainerImpl* annotation_container =
          AnnotationAgentContainerImpl::CreateIfNeeded(
              *selected_frame->GetDocument());
      annotation_container->OpenedContextMenuOverSelection();
    }
  }

  // If there is a text fragment at the same location as the click indicate that
  // the context menu is being opened from an existing highlight.
  if (result.InnerNodeFrame()) {
    result.InnerNodeFrame()->View()->UpdateLifecycleToPrePaintClean(
        DocumentUpdateReason::kHitTest);
    if (TextFragmentHandler::IsOverTextFragment(result)) {
      data.opened_from_highlight = true;
    }
  }

  if (result.IsContentEditable()) {
    data.is_editable = true;
    SpellChecker& spell_checker = selected_frame->GetSpellChecker();

    // Spellchecker adds spelling markers to misspelled words and attaches
    // suggestions to these markers in the background. Therefore, when a
    // user right-clicks a mouse on a word, Chrome just needs to find a
    // spelling marker on the word instead of spellchecking it.
    std::pair<String, String> misspelled_word_and_description =
        spell_checker.SelectMisspellingAsync();
    const String& misspelled_word = misspelled_word_and_description.first;
    if (misspelled_word.length()) {
      auto to_u16string = [](const String& s) -> std::u16string {
        return s.empty() ? std::u16string()
                         : WTF::VisitCharacters(s, [](auto chars) {
                             return std::u16string(chars.begin(), chars.end());
                           });
      };
      data.misspelled_word = to_u16string(misspelled_word);
      const String& description = misspelled_word_and_description.second;
      if (description.length()) {
        // Suggestions were cached for the misspelled word (won't be true for
        // Hunspell, or Windows platform spellcheck if the
        // kWinRetrieveSuggestionsOnlyOnDemand feature flag is set).
        Vector<String> suggestions;
        description.Split('\n', suggestions);
        WebVector<std::u16string> web_suggestions(suggestions.size());
        base::ranges::transform(suggestions, web_suggestions.begin(),
                                to_u16string);
        data.dictionary_suggestions = web_suggestions.ReleaseVector();
      } else if (spell_checker.GetTextCheckerClient()) {
        // No suggestions cached for the misspelled word. Retrieve suggestions
        // for it (Windows platform spellchecker will do this later from
        // SpellingMenuObserver::InitMenu on the browser process side to avoid a
        // blocking IPC here).
        size_t misspelled_offset, misspelled_length;
        WebVector<WebString> web_suggestions;
        spell_checker.GetTextCheckerClient()->CheckSpelling(
            WebString::FromUTF16(data.misspelled_word), misspelled_offset,
            misspelled_length, &web_suggestions);
        WebVector<std::u16string> suggestions(web_suggestions.size());
        base::ranges::transform(web_suggestions, suggestions.begin(),
                                &WebString::Utf16);
        data.dictionary_suggestions = suggestions.ReleaseVector();
      }
    }
  }

  if (EditingStyle::SelectionHasStyle(*selected_frame,
                                      CSSPropertyID::kDirection,
                                      "ltr") != EditingTriState::kFalse) {
    data.writing_direction_left_to_right |=
        ContextMenuData::kCheckableMenuItemChecked;
  }
  if (EditingStyle::SelectionHasStyle(*selected_frame,
                                      CSSPropertyID::kDirection,
                                      "rtl") != EditingTriState::kFalse) {
    data.writing_direction_right_to_left |=
        ContextMenuData::kCheckableMenuItemChecked;
  }

  if (Document* doc = selected_frame->GetDocument()) {
    data.is_image_media_plugin_document = doc->IsImageDocument() ||
                                          doc->IsMediaDocument() ||
                                          doc->IsPluginDocument();
  }
  data.referrer_policy = selected_frame->DomWindow()->GetReferrerPolicy();

  if (menu_provider_) {
    // Filter out custom menu elements and add them into the data.
    data.custom_items = menu_provider_->PopulateContextMenu().ReleaseVector();
  }

  // TODO(crbug.com/369219144): Should this be DynamicTo<HTMLAnchorElementBase>?
  if (auto* anchor = DynamicTo<HTMLAnchorElement>(result.URLElement())) {
    // Extract suggested filename for same-origin URLS for saving file.
    const SecurityOrigin* origin =
        selected_frame->GetSecurityContext()->GetSecurityOrigin();
    if (origin->CanReadContent(anchor->Url())) {
      data.suggested_filename =
          anchor->FastGetAttribute(html_names::kDownloadAttr).Utf8();
    }

    // If the anchor wants to suppress the referrer, update the referrerPolicy
    // accordingly.
    if (anchor->HasRel(kRelationNoReferrer))
      data.referrer_policy = network::mojom::ReferrerPolicy::kNever;

    data.link_text = anchor->innerText().Utf8();

    if (const AtomicString& attribution_src_value =
            anchor->FastGetAttribute(html_names::kAttributionsrcAttr);
        !attribution_src_value.IsNull()) {
      // TODO(crbug.com/1381123): Support background attributionsrc requests
      // if attribute value is non-empty.

      // An impression should be attached to the navigation regardless of
      // whether a background request would have been allowed or attempted.
      if (!data.impression) {
        if (AttributionSrcLoader* attribution_src_loader =
                selected_frame->GetAttributionSrcLoader();
            attribution_src_loader->CanRegister(result.AbsoluteLinkURL(),
                                                /*element=*/anchor,
                                                /*request_id=*/std::nullopt)) {
          data.impression = blink::Impression();
        }
      }
    }
  }

  data.selection_rect = ComputeSelectionRect(selected_frame);
  data.source_type = source_type;

  SetAutofillData(result.InnerNode(), data);
  SetPasswordManagerData(result.InnerElement(), data);

  const bool from_touch = source_type == kMenuSourceTouch ||
                          source_type == kMenuSourceLongPress ||
                          source_type == kMenuSourceLongTap;
  if (from_touch && !ShouldShowContextMenuFromTouch(data))
    return false;

  WebLocalFrameImpl* selected_web_frame =
      WebLocalFrameImpl::FromFrame(selected_frame);
  if (!selected_web_frame || !selected_web_frame->Client())
    return false;

  std::optional<gfx::Point> host_context_menu_location;
  if (selected_web_frame->FrameWidgetImpl()) {
    host_context_menu_location =
        selected_web_frame->FrameWidgetImpl()->GetAndResetContextMenuLocation();
  }
  if (!host_context_menu_location.has_value()) {
    auto* main_frame =
        WebLocalFrameImpl::FromFrame(DynamicTo<LocalFrame>(page_->MainFrame()));
    if (main_frame && main_frame != selected_web_frame) {
      host_context_menu_location =
          main_frame->FrameWidgetImpl()->GetAndResetContextMenuLocation();
    }
  }

  selected_web_frame->ShowContextMenu(
      context_menu_client_receiver_.BindNewEndpointAndPassRemote(
          selected_web_frame->GetTaskRunner(TaskType::kInternalDefault)),
      data, host_context_menu_location);

  return true;
}

}  // namespace blink
