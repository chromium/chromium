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
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using blink::WebLocalizedString;
using namespace HTMLNames;

namespace {

WebVector<WebString> CollectAcceptTypes(const HTMLInputElement& input) {
  Vector<String> mime_types = input.AcceptMIMETypes();
  Vector<String> extensions = input.AcceptFileExtensions();

  Vector<String> accept_types;
  accept_types.ReserveCapacity(mime_types.size() + extensions.size());
  accept_types.AppendVector(mime_types);
  accept_types.AppendVector(extensions);
  return accept_types;
}

}  // namespace

inline FileInputType::FileInputType(HTMLInputElement& element)
    : InputType(element),
      KeyboardClickableInputTypeView(element),
      file_list_(FileList::Create()) {}

InputType* FileInputType::Create(HTMLInputElement& element) {
  return new FileInputType(element);
}

void FileInputType::Trace(blink::Visitor* visitor) {
  visitor->Trace(file_list_);
  KeyboardClickableInputTypeView::Trace(visitor);
  InputType::Trace(visitor);
}

InputTypeView* FileInputType::CreateView() {
  return this;
}

FileChooserFileInfoList FileInputType::FilesFromFormControlState(
    const FormControlState& state) {
  FileChooserFileInfoList files;
  for (wtf_size_t i = 0; i < state.ValueSize(); i += 2) {
    if (!state[i + 1].IsEmpty())
      files.push_back(CreateFileChooserFileInfoNative(state[i], state[i + 1]));
    else
      files.push_back(CreateFileChooserFileInfoNative(state[i]));
  }
  return files;
}

const AtomicString& FileInputType::FormControlType() const {
  return InputTypeNames::file;
}

FormControlState FileInputType::SaveFormControlState() const {
  if (file_list_->IsEmpty())
    return FormControlState();
  FormControlState state;
  unsigned num_files = file_list_->length();
  for (unsigned i = 0; i < num_files; ++i) {
    if (file_list_->item(i)->HasBackingFile()) {
      state.Append(file_list_->item(i)->GetPath());
      state.Append(file_list_->item(i)->name());
    }
    // FIXME: handle Blob-backed File instances, see http://crbug.com/394948
  }
  return state;
}

void FileInputType::RestoreFormControlState(const FormControlState& state) {
  if (state.ValueSize() % 2)
    return;
  FilesChosen(FilesFromFormControlState(state));
}

void FileInputType::AppendToFormData(FormData& form_data) const {
  FileList* file_list = GetElement().files();
  unsigned num_files = file_list->length();
  if (num_files == 0) {
    form_data.AppendFromElement(GetElement().GetName(), File::Create(""));
    return;
  }

  for (unsigned i = 0; i < num_files; ++i) {
    form_data.AppendFromElement(GetElement().GetName(), file_list->item(i));
  }
}

bool FileInputType::ValueMissing(const String& value) const {
  return GetElement().IsRequired() && value.IsEmpty();
}

String FileInputType::ValueMissingText() const {
  return GetLocale().QueryString(
      GetElement().Multiple()
          ? WebLocalizedString::kValidationValueMissingForMultipleFile
          : WebLocalizedString::kValidationValueMissingForFile);
}

void FileInputType::HandleDOMActivateEvent(Event& event) {
  if (GetElement().IsDisabledFormControl())
    return;

  if (!LocalFrame::HasTransientUserActivation(
          GetElement().GetDocument().GetFrame()))
    return;

  if (ChromeClient* chrome_client = GetChromeClient()) {
    WebFileChooserParams params;
    HTMLInputElement& input = GetElement();
    Document& document = input.GetDocument();
    bool is_directory = input.FastHasAttribute(webkitdirectoryAttr);
    if (is_directory)
      params.mode = WebFileChooserParams::Mode::kUploadFolder;
    else if (input.FastHasAttribute(multipleAttr))
      params.mode = WebFileChooserParams::Mode::kOpenMultiple;
    else
      params.mode = WebFileChooserParams::Mode::kOpen;
    params.need_local_path = is_directory;
    params.accept_types = CollectAcceptTypes(input);
    params.selected_files = file_list_->PathsForUserVisibleFiles();
    params.use_media_capture = RuntimeEnabledFeatures::MediaCaptureEnabled() &&
                               input.FastHasAttribute(captureAttr);
    params.requestor = document.Url();

    UseCounter::Count(
        document, document.IsSecureContext()
                      ? WebFeature::kInputTypeFileSecureOriginOpenChooser
                      : WebFeature::kInputTypeFileInsecureOriginOpenChooser);

    chrome_client->OpenFileChooser(document.GetFrame(), NewFileChooser(params));
  }
  event.SetDefaultHandled();
}

LayoutObject* FileInputType::CreateLayoutObject(const ComputedStyle&) const {
  return new LayoutFileUploadControl(&GetElement());
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
  return value.IsEmpty();
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
  GetElement().SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kControlValue));
  GetElement().SetNeedsValidityCheck();
}

FileList* FileInputType::CreateFileList(const FileChooserFileInfoList& files,
                                        bool has_webkit_directory_attr) {
  FileList* file_list(FileList::Create());
  wtf_size_t size = files.size();

  // If a directory is being selected, the UI allows a directory to be chosen
  // and the paths provided here share a root directory somewhere up the tree;
  // we want to store only the relative paths from that point.
  if (size && has_webkit_directory_attr) {
    // Find the common root path.
    base::FilePath root_path = files[0]->get_native_file()->file_path.DirName();
    for (wtf_size_t i = 1; i < size; ++i) {
      while (files[i]->get_native_file()->file_path.value().find(
                 root_path.value()) != 0)
        root_path = root_path.DirName();
    }
    root_path = root_path.DirName();
    int root_length = FilePathToString(root_path).length();
    DCHECK(root_length);
    if (!root_path.EndsWithSeparator())
      root_length += 1;
    for (const auto& file : files) {
      // Normalize backslashes to slashes before exposing the relative path to
      // script.
      String string_path = FilePathToString(file->get_native_file()->file_path);
      String relative_path =
          string_path.Substring(root_length).Replace('\\', '/');
      file_list->Append(
          File::CreateWithRelativePath(string_path, relative_path));
    }
    return file_list;
  }

  for (const auto& file : files) {
    if (file->is_native_file()) {
      file_list->Append(File::CreateForUserProvidedFile(
          FilePathToString(file->get_native_file()->file_path),
          file->get_native_file()->display_name));
    } else {
      const auto& fs_info = file->get_file_system();
      FileMetadata metadata;
      metadata.modification_time = fs_info->modification_time.ToJsTime();
      metadata.length = fs_info->length;
      metadata.type = FileMetadata::kTypeFile;
      file_list->Append(File::CreateForFileSystemFile(fs_info->url, metadata,
                                                      File::kIsUserVisible));
    }
  }
  return file_list;
}

void FileInputType::CountUsage() {
  Document* document = &GetElement().GetDocument();
  if (document->IsSecureContext())
    UseCounter::Count(*document, WebFeature::kInputTypeFileInsecureOrigin);
  else
    UseCounter::Count(*document, WebFeature::kInputTypeFileSecureOrigin);
}

void FileInputType::CreateShadowSubtree() {
  DCHECK(IsShadowHost(GetElement()));
  auto* button = HTMLInputElement::Create(GetElement().GetDocument(),
                                          CreateElementFlags());
  button->setType(InputTypeNames::button);
  button->setAttribute(
      valueAttr,
      AtomicString(GetLocale().QueryString(
          GetElement().Multiple()
              ? WebLocalizedString::kFileButtonChooseMultipleFilesLabel
              : WebLocalizedString::kFileButtonChooseFileLabel)));
  button->SetShadowPseudoId(AtomicString("-webkit-file-upload-button"));
  GetElement().UserAgentShadowRoot()->AppendChild(button);
}

void FileInputType::DisabledAttributeChanged() {
  DCHECK(IsShadowHost(GetElement()));
  if (Element* button =
          ToElementOrDie(GetElement().UserAgentShadowRoot()->firstChild()))
    button->SetBooleanAttribute(disabledAttr,
                                GetElement().IsDisabledFormControl());
}

void FileInputType::MultipleAttributeChanged() {
  DCHECK(IsShadowHost(GetElement()));
  if (Element* button =
          ToElementOrDie(GetElement().UserAgentShadowRoot()->firstChild()))
    button->setAttribute(
        valueAttr,
        AtomicString(GetLocale().QueryString(
            GetElement().Multiple()
                ? WebLocalizedString::kFileButtonChooseMultipleFilesLabel
                : WebLocalizedString::kFileButtonChooseFileLabel)));
}

void FileInputType::SetFiles(FileList* files) {
  if (!files)
    return;

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

  if (GetElement().GetLayoutObject())
    GetElement().GetLayoutObject()->SetShouldDoFullPaintInvalidation();

  if (files_changed) {
    // This call may cause destruction of this instance.
    // input instance is safe since it is ref-counted.
    GetElement().DispatchInputEvent();
    GetElement().DispatchChangeEvent();
  }
}

void FileInputType::FilesChosen(const FileChooserFileInfoList& files) {
  SetFiles(CreateFileList(files,
                          GetElement().FastHasAttribute(webkitdirectoryAttr)));
  if (HasConnectedFileChooser())
    DisconnectFileChooser();
}

LocalFrame* FileInputType::FrameOrNull() const {
  return GetElement().GetDocument().GetFrame();
}

void FileInputType::SetFilesFromDirectory(const String& path) {
  if (ChromeClient* chrome_client = GetChromeClient()) {
    Vector<String> files;
    files.push_back(path);
    WebFileChooserParams params;
    params.mode = WebFileChooserParams::Mode::kUploadFolder;
    params.selected_files = files;
    params.accept_types = CollectAcceptTypes(GetElement());
    params.requestor = GetElement().GetDocument().Url();
    chrome_client->EnumerateChosenDirectory(NewFileChooser(params));
  }
}

void FileInputType::SetFilesFromPaths(const Vector<String>& paths) {
  if (paths.IsEmpty())
    return;

  HTMLInputElement& input = GetElement();
  if (input.FastHasAttribute(webkitdirectoryAttr)) {
    SetFilesFromDirectory(paths[0]);
    return;
  }

  FileChooserFileInfoList files;
  for (const auto& path : paths)
    files.push_back(CreateFileChooserFileInfoNative(path));

  if (input.FastHasAttribute(multipleAttr)) {
    FilesChosen(files);
  } else {
    FileChooserFileInfoList first_file_only;
    first_file_only.push_back(std::move(files[0]));
    FilesChosen(first_file_only);
  }
}

bool FileInputType::ReceiveDroppedFiles(const DragData* drag_data) {
  Vector<String> paths;
  drag_data->AsFilePaths(paths);
  if (paths.IsEmpty())
    return false;

  if (!GetElement().FastHasAttribute(webkitdirectoryAttr)) {
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
    return GetLocale().QueryString(
        WebLocalizedString::kFileButtonNoFileSelectedLabel);
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
  if (GetElement().FastHasAttribute(webkitdirectoryAttr)) {
    // Override to invoke the action on Enter key up (not press) to avoid
    // repeats committing the file chooser.
    if (event.key() == "Enter") {
      event.SetDefaultHandled();
      return;
    }
  }
  KeyboardClickableInputTypeView::HandleKeypressEvent(event);
}

void FileInputType::HandleKeyupEvent(KeyboardEvent& event) {
  if (GetElement().FastHasAttribute(webkitdirectoryAttr)) {
    // Override to invoke the action on Enter key up (not press) to avoid
    // repeats committing the file chooser.
    if (event.key() == "Enter") {
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

}  // namespace blink
