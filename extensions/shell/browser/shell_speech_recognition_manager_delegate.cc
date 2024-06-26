// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_speech_recognition_manager_delegate.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"

using content::BrowserThread;
using content::SpeechRecognitionManager;
using content::WebContents;

namespace extensions {
namespace speech {

ShellSpeechRecognitionManagerDelegate::ShellSpeechRecognitionManagerDelegate() {
}

ShellSpeechRecognitionManagerDelegate::
~ShellSpeechRecognitionManagerDelegate() {
}

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
void ShellSpeechRecognitionManagerDelegate::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {}
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)

void ShellSpeechRecognitionManagerDelegate::OnRecognitionStart(int session_id) {
}

void ShellSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {
}

void ShellSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {
}

void ShellSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {
}

void ShellSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {
}

void ShellSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {
}

void ShellSpeechRecognitionManagerDelegate::OnRecognitionResults(
    int session_id,
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& result) {}

void ShellSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id,
    const media::mojom::SpeechRecognitionError& error) {}

void ShellSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id,
    float volume,
    float noise_volume) {
}

void ShellSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // Make sure that initiators (extensions/web pages) properly set the
  // |render_process_id| field, which is needed later to retrieve the profile.
  DCHECK_NE(context.render_process_id, 0);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CheckRenderFrameType, std::move(callback),
                     context.render_process_id, context.render_frame_id));
}

content::SpeechRecognitionEventListener*
ShellSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

// static
void ShellSpeechRecognitionManagerDelegate::CheckRenderFrameType(
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  bool allowed = false;
  bool check_permission = false;

  if (render_frame_host) {
    WebContents* web_contents =
        WebContents::FromRenderFrameHost(render_frame_host);
    extensions::mojom::ViewType view_type =
        extensions::GetViewType(web_contents);

    if (view_type == extensions::mojom::ViewType::kAppWindow ||
        view_type == extensions::mojom::ViewType::kExtensionBackgroundPage) {
      allowed = true;
      check_permission = true;
    } else {
      LOG(WARNING) << "Speech recognition only supported in Apps.";
    }
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), check_permission, allowed));
}

}  // namespace speech
}  // namespace extensions
