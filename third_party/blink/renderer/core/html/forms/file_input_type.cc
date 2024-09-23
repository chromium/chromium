/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/file_input_type.h"

#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::FileChooserParams;

namespace {

Vector<String> CollectAcceptTypes(const HTMLInputElement& input) {
  Vector<String> mime_types = input.AcceptMIMETypes();
  Vector<String> extensions = input.AcceptFileExtensions();

  Vector<String> accept_types;
  accept_types.reserve(mime_types.size() + extensions.size());
  accept_types.AppendVector(mime_types);
  accept_types.AppendVector(extensions);
  return accept_types;
}

}  // namespace

FileInputType::FileInputType(HTMLInputElement& element)
    : InputType(Type::kFile, element),
      KeyboardClickableInputTypeView(element),
      file_list_(MakeGarbageCollected<FileList>()) {}

void FileInputType::Trace(Visitor* visitor) const {
  visitor->Trace(file_list_);
  KeyboardClickableInputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* FileInputType::CreateView() {
  return this;
}

template <typename ItemType, typename VectorType>
VectorType CreateFilesFrom(const FormControlState& state,
                           ItemType (*factory)(const FormControlState&,
                                               wtf_size_t&)) {
  VectorType files;
  files.ReserveInitialCapacity(state.ValueSize() / 3);
  for (wtf_size_t i = 0; i < state.ValueSize();) {
    files.push_back(factory(state, i));
  }
  return files;
}

template <typename ItemType, typename VectorType>
VectorType CreateFilesFrom(const FormControlState& state,
                           ExecutionContext* execution_context,
                           ItemType (*factory)(ExecutionContext*,
                                               const FormControlState&,
                                               wtf_size_t&)) {
  VectorType files;
  files.ReserveInitialCapacity(state.ValueSize() / 3);
  for (wtf_size_t i = 0; i < state.ValueSize();) {
    files.push_back(factory(execution_context, state, i));
  }
  return files;
}

Vector<String> FileInputType::FilesFromFormControlState(
    const FormControlState& state) {
  return CreateFilesFrom<String, Vector<String>>(state,
                                                 &File::PathFromControlState);
}

FormControlState FileInputType::SaveFormControlState() const {
  if (file_list_->IsEmpty() ||
      GetElement().GetDocument().GetFormController().DropReferencedFilePaths())
    return FormControlState();
  FormControlState state;
  unsigned num_files = file_list_->length();
  for (unsigned i = 0; i < num_files; ++i)
    file_list_->item(i)->AppendToControlState(state);
  return state;
}

void FileInputType::RestoreFormControlState(const FormControlState& state) {
  if (state.ValueSize() % 3)
    return;
  ExecutionContext* execution_context = GetElement().GetExecutionContext();
  HeapVector<Member<File>> file_vector =
      CreateFilesFrom<File*, HeapVector<Member<File>>>(
          state, execution_context, &File::CreateFromControlState);
  auto* file_list = MakeGarbageCollected<FileList>();
  for (const auto& file : file_vector)
    file_list->Append(file);
  SetFiles(file_list);
}

void FileInputType::AppendToFormData(FormData& form_data) const {
  FileList* file_list = GetElement().files();
  unsigned num_files = file_list->length();
  ExecutionContext* context = GetElement().GetExecutionContext();
  if (num_files == 0) {
    form_data.AppendFromElement(GetElement().GetName(),
                                MakeGarbageCollected<File>(context, ""));
    return;
  }

  for (unsigned i = 0; i < num_files; ++i) {
    form_data.AppendFromElement(GetElement().GetName(), file_list->item(i));
  }
}

bool FileInputType::ValueMissing(const String& value) const {
  return GetElement().IsRequired() && value.empty();
}

String FileInputType::ValueMissingText() const {
  return GetLocale().QueryString(
      GetElement().Multiple() ? IDS_FORM_VALIDATION_VALUE_MISSING_MULTIPLE_FILE
                              : IDS_FORM_VALIDATION_VALUE_MISSING_FILE);
}

void FileInputType::HandleDOMActivateEvent(Event& event) {
  if (GetElement().IsDisabledFormControl())
    return;

  HTMLInputElement& input = GetElement();
  Document& document = input.GetDocument();

  if (!LocalFrame::HasTransientUserActivation(document.GetFrame())) {
    String message =
        "File chooser dialog can only be shown with a user activation.";
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning, message));
    return;
  }

  OpenPopupView();
  event.SetDefaultHandled();
}

void FileInputType::OpenPopupView() {
  HTMLInputElement& input = GetElement();
  Document& document = input.GetDocument();

  bool intercepted = false;
  probe::FileChooserOpened(document.GetFrame(), &input, input.Multiple(),
                           &intercepted);
  if (intercepted) {
    return;
  }

  if (ChromeClient* chrome_client = GetChromeClient()) {
    FileChooserParams params;
    bool is_directory =
        input.FastHasAttribute(html_names::kWebkitdirectoryAttr);
    if (is_directory)
      params.mode = FileChooserParams::Mode::kUploadFolder;
    else if (input.FastHasAttribute(html_names::kMultipleAttr))
      params.mode = FileChooserParams::Mode::kOpenMultiple;
    else
      params.mode = FileChooserParams::Mode::kOpen;
    params.title = g_empty_string;
    params.need_local_path = is_directory;
    params.accept_types = CollectAcceptTypes(input);
    params.selected_files = file_list_->PathsForUserVisibleFiles();
    params.use_media_capture = RuntimeEnabledFeatures::MediaCaptureEnabled() &&
                               input.FastHasAttribute(html_names::kCaptureAttr);
    params.requestor = document.Url();

    UseCounter::Count(
        document, GetElement().GetExecutionContext()->IsSecureContext()
                      ? WebFeature::kInputTypeFileSecureOriginOpenChooser
                      : WebFeature::kInputTypeFileInsecureOriginOpenChooser);
    chrome_client->OpenFileChooser(document.GetFrame(), NewFileChooser(params));
  }
}

void FileInputType::AdjustStyle(ComputedStyleBuilder& builder) {
  builder.SetShouldIgnoreOverflowPropertyForInlineBlockBaseline();
}

LayoutObject* FileInputType::CreateLayoutObject(const ComputedStyle&) const {
  return MakeGarbageCollected<LayoutBlockFlow>(&GetElement());
}

InputType::ValueMode FileInputType::GetValueMode() const {
  return ValueMode::kFilename;
}

bool FileInputType::CanSetStringValue() const {
  return false;
}

FileList* FileInputType::Files() {
  return file_list_.Get();
}

bool FileInputType::CanSetValue(const String& value) {
  // For security reasons, we don't allow setting the filename, but we do allow
  // clearing it.  The HTML5 spec (as of the 10/24/08 working draft) says that
  // the value attribute isn't applicable to the file upload control at all, but
  // for now we are keeping this behavior to avoid breaking existing websites
  // that may be relying on this.
  return value.empty();
}

String FileInputType::ValueInFilenameValueMode() const {
  if (file_list_->IsEmpty())
    return String();

  // HTML5 tells us that we're supposed to use this goofy value for
  // file input controls. Historically, browsers revealed the real
  // file path, but that's a privacy problem. Code on the web
  // decided to try to parse the value by looking for backslashes
  // (because that's what Windows file paths use). To be compatible
  // with that code, we make up a fake path for the file.
  return "C:\\fakepath\\" + file_list_->item(0)->name();
}

void FileInputType::SetValue(const String&,
                             bool value_changed,
                             TextFieldEventBehavior,
                             TextControlSetValueSelection) {
  if (!value_changed)
    return;

  file_list_->clear();
  GetElement().SetNeedsValidityCheck();
  UpdateView();
}

FileList* FileInputType::CreateFileList(ExecutionContext& context,
                                        const FileChooserFileInfoList& files,
                                        const base::FilePath& base_dir) {
  auto* file_list(MakeGarbageCollected<FileList>());
  wtf_size_t size = files.size();

  // If a directory is being selected, the UI allows a directory to be chosen
  // and the paths provided here should start with |base_dir|.
  // We want to store only the relative path starting with the basename of
  // |base_dir|.
  if (size && !base_dir.empty()) {
    base::FilePath root_path = base_dir.DirName();
    int root_length = FilePathToString(root_path).length();
    DCHECK(root_length);
    if (!root_path.EndsWithSeparator())
      root_length += 1;
    if (base_dir == root_path)
      root_length = 0;
    for (const auto& file : files) {
      // Normalize backslashes to slashes before exposing the relative path to
      // script.
      String string_path = FilePathToString(file->get_native_file()->file_path);
      DCHECK(
          string_path.StartsWithIgnoringASCIICase(FilePathToString(base_dir)))
          << "A path in a FileChooserFileInfo " << string_path
          << " should start with " << FilePathToString(base_dir);
      String relative_path =
          string_path.Substring(root_length).Replace('\\', '/');
      file_list->Append(
          File::CreateWithRelativePath(&context, string_path, relative_path));
    }
    return file_list;
  }

  for (const auto& file : files) {
    if (file->is_native_file()) {
      file_list->Append(File::CreateForUserProvidedFile(
          &context, FilePathToString(file->get_native_file()->file_path),
          file->get_native_file()->display_name));
    } else {
      const auto& fs_info = file->get_file_system();
      FileMetadata metadata;
      metadata.modification_time =
          NullableTimeToOptionalTime(fs_info->modification_time);
      metadata.length = fs_info->length;
      metadata.type = FileMetadata::kTypeFile;
      file_list->Append(File::CreateForFileSystemFile(
          context, fs_info->url, metadata, File::kIsUserVisible));
    }
  }
  return file_list;
}

void FileInputType::CountUsage() {
  ExecutionContext* context = GetElement().GetExecutionContext();
  if (context->IsSecureContext())
    UseCounter::Count(context, WebFeature::kInputTypeFileSecureOrigin);
  else
    UseCounter::Count(context, WebFeature::kInputTypeFileInsecureOrigin);
}

void FileInputType::CreateShadowSubtree() {
  DCHECK(IsShadowHost(GetElement()));
  Document& document = GetElement().GetDocument();

  auto* button = MakeGarbageCollected<HTMLInputElement>(document);
  button->setType(input_type_names::kButton);
  button->setAttribute(
      html_names::kValueAttr,
      AtomicString(GetLocale().QueryString(
          GetElement().Multiple() ? IDS_FORM_MULTIPLE_FILES_BUTTON_LABEL
                                  : IDS_FORM_FILE_BUTTON_LABEL)));
  button->SetShadowPseudoId(shadow_element_names::kPseudoFileUploadButton);
  button->setAttribute(html_names::kIdAttr,
                       shadow_element_names::kIdFileUploadButton);
  button->SetActive(GetElement().CanReceiveDroppedFiles());
  GetElement().UserAgentShadowRoot()->AppendChild(button);

  auto* span = document.CreateRawElement(html_names::kSpanTag);
  GetElement().UserAgentShadowRoot()->AppendChild(span);

  // The file input element is presented to AX as one node with the role button,
  // instead of the individual button and text nodes. That's the reason we hide
  // the shadow root elements of the file input in the AX tree.
  button->setAttribute(html_names::kAriaHiddenAttr, keywords::kTrue);
  span->setAttribute(html_names::kAriaHiddenAttr, keywords::kTrue);

  UpdateView();
}

HTMLInputElement* FileInputType::UploadButton() const {
  Element* element = GetElement().EnsureShadowSubtree()->getElementById(
      shadow_element_names::kIdFileUploadButton);
  CHECK(!element || IsA<HTMLInputElement>(element));
  return To<HTMLInputElement>(element);
}

Node* FileInputType::FileStatusElement() const {
  return GetElement().EnsureShadowSubtree()->lastChild();
}

void FileInputType::DisabledAttributeChanged() {
  DCHECK(RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled() ||
         IsShadowHost(GetElement()));
  if (Element* button = UploadButton()) {
    button->SetBooleanAttribute(html_names::kDisabledAttr,
                                GetElement().IsDisabledFormControl());
  }
}

void FileInputType::MultipleAttributeChanged() {
  DCHECK(RuntimeEnabledFeatures::CreateInputShadowTreeDuringLayoutEnabled() ||
         IsShadowHost(GetElement()));
  if (Element* button = UploadButton()) {
    button->setAttribute(
        html_names::kValueAttr,
        AtomicString(GetLocale().QueryString(
            GetElement().Multiple() ? IDS_FORM_MULTIPLE_FILES_BUTTON_LABEL
                                    : IDS_FORM_FILE_BUTTON_LABEL)));
  }
}

bool FileInputType::SetFiles(FileList* files) {
  if (!files)
    return false;

  bool files_changed = false;
  if (files->length() != file_list_->length()) {
    files_changed = true;
  } else {
    for (unsigned i = 0; i < files->length(); ++i) {
      if (!files->item(i)->HasSameSource(*file_list_->item(i))) {
        files_changed = true;
        break;
      }
    }
  }

  file_list_ = files;

  GetElement().NotifyFormStateChanged();
  GetElement().SetNeedsValidityCheck();
  UpdateView();
  return files_changed;
}

void FileInputType::SetFilesAndDispatchEvents(FileList* files) {
  if (SetFiles(files)) {
    // This call may cause destruction of this instance.
    // input instance is safe since it is ref-counted.
    GetElement().DispatchInputEvent();
    GetElement().DispatchChangeEvent();
    if (AXObjectCache* cache =
            GetElement().GetDocument().ExistingAXObjectCache()) {
      cache->HandleValueChanged(&GetElement());
    }
  } else {
    GetElement().DispatchCancelEvent();
  }
}

void FileInputType::FilesChosen(FileChooserFileInfoList files,
                                const base::FilePath& base_dir) {
  for (wtf_size_t i = 0; i < files.size();) {
    // Drop files of which names can not be converted to WTF String. We
    // can't expose such files via File API.
    if (files[i]->is_native_file() &&
        FilePathToString(files[i]->get_native_file()->file_path).empty()) {
      files.EraseAt(i);
      // Do not increment |i|.
      continue;
    }
    ++i;
  }
  if (!will_be_destroyed_) {
    SetFilesAndDispatchEvents(
        CreateFileList(*GetElement().GetExecutionContext(), files, base_dir));
  }
  if (HasConnectedFileChooser())
    DisconnectFileChooser();
}

LocalFrame* FileInputType::FrameOrNull() const {
  return GetElement().GetDocument().GetFrame();
}

void FileInputType::SetFilesFromDirectory(const String& path) {
  FileChooserParams params;
  params.mode = FileChooserParams::Mode::kUploadFolder;
  params.title = g_empty_string;
  params.selected_files.push_back(StringToFilePath(path));
  params.accept_types = CollectAcceptTypes(GetElement());
  params.requestor = GetElement().GetDocument().Url();
  NewFileChooser(params)->EnumerateChosenDirectory();
}

void FileInputType::SetFilesFromPaths(const Vector<String>& paths) {
  if (paths.empty())
    return;

  HTMLInputElement& input = GetElement();
  if (input.FastHasAttribute(html_names::kWebkitdirectoryAttr)) {
    SetFilesFromDirectory(paths[0]);
    return;
  }

  FileChooserFileInfoList files;
  for (const auto& path : paths)
    files.push_back(CreateFileChooserFileInfoNative(path));

  if (input.FastHasAttribute(html_names::kMultipleAttr)) {
    FilesChosen(std::move(files), base::FilePath());
  } else {
    FileChooserFileInfoList first_file_only;
    first_file_only.push_back(std::move(files[0]));
    FilesChosen(std::move(first_file_only), base::FilePath());
  }
}

bool FileInputType::ReceiveDroppedFiles(const DragData* drag_data) {
  Vector<String> paths;
  drag_data->AsFilePaths(paths);
  if (paths.empty())
    return false;

  if (!GetElement().FastHasAttribute(html_names::kWebkitdirectoryAttr)) {
    dropped_file_system_id_ = drag_data->DroppedFileSystemId();
  }
  SetFilesFromPaths(paths);
  return true;
}

String FileInputType::DroppedFileSystemId() {
  return dropped_file_system_id_;
}

String FileInputType::DefaultToolTip(const InputTypeView&) const {
  FileList* file_list = file_list_.Get();
  unsigned list_size = file_list->length();
  if (!list_size) {
    return GetLocale().QueryString(IDS_FORM_FILE_NO_FILE_LABEL);
  }

  StringBuilder names;
  for (wtf_size_t i = 0; i < list_size; ++i) {
    names.Append(file_list->item(i)->name());
    if (i != list_size - 1)
      names.Append('\n');
  }
  return names.ToString();
}

void FileInputType::CopyNonAttributeProperties(const HTMLInputElement& source) {
  DCHECK(file_list_->IsEmpty());
  const FileList* source_list = source.files();
  for (unsigned i = 0; i < source_list->length(); ++i)
    file_list_->Append(source_list->item(i)->Clone());
}

void FileInputType::HandleKeypressEvent(KeyboardEvent& event) {
  if (GetElement().FastHasAttribute(html_names::kWebkitdirectoryAttr)) {
    // Override to invoke the action on Enter key up (not press) to avoid
    // repeats committing the file chooser.
    if (event.key() == keywords::kCapitalEnter) {
      event.SetDefaultHandled();
      return;
    }
  }
  KeyboardClickableInputTypeView::HandleKeypressEvent(event);
}

void FileInputType::HandleKeyupEvent(KeyboardEvent& event) {
  if (GetElement().FastHasAttribute(html_names::kWebkitdirectoryAttr)) {
    // Override to invoke the action on Enter key up (not press) to avoid
    // repeats committing the file chooser.
    if (event.key() == keywords::kCapitalEnter) {
      GetElement().DispatchSimulatedClick(&event);
      event.SetDefaultHandled();
      return;
    }
  }
  KeyboardClickableInputTypeView::HandleKeyupEvent(event);
}

void FileInputType::WillOpenPopup() {
  // TODO(tkent): Should we disconnect the file chooser? crbug.com/637639
  if (HasConnectedFileChooser()) {
    UseCounter::Count(GetElement().GetDocument(),
                      WebFeature::kPopupOpenWhileFileChooserOpened);
  }
}

String FileInputType::FileStatusText() const {
  Locale& locale = GetLocale();

  if (file_list_->IsEmpty())
    return locale.QueryString(IDS_FORM_FILE_NO_FILE_LABEL);

  if (file_list_->length() == 1)
    return LayoutTheme::GetTheme().DisplayNameForFile(*file_list_->item(0));

  return locale.QueryString(
      IDS_FORM_FILE_MULTIPLE_UPLOAD,
      locale.ConvertToLocalizedNumber(String::Number(file_list_->length())));
}

void FileInputType::UpdateView() {
  if (auto* span = FileStatusElement())
    span->setTextContent(FileStatusText());
}

}  // namespace blink
