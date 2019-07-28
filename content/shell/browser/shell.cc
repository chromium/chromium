// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell.h"
#include "chrome/browser/file_select_helper.h"

#ifdef _WIN32
#include <windows.h>
#include <string>
#include <shlobj.h>
#include <iostream>
#include <sstream>
#include <commdlg.h>
#endif

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "content/shell/browser/web_test/blink_test_controller.h"
#include "content/shell/browser/web_test/fake_bluetooth_scanning_prompt.h"
#include "content/shell/browser/web_test/secondary_test_window_observer.h"
#include "content/shell/browser/web_test/web_test_bluetooth_chooser_factory.h"
#include "content/shell/browser/web_test/web_test_devtools_bindings.h"
#include "content/shell/browser/web_test/web_test_javascript_dialog_manager.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/web/web_presentation_receiver_flags.h"
#include "chrome/browser/file_select_helper.h"

#include "content/public/common/file_chooser_file_info.h"

namespace content {

// Null until/unless the default main message loop is running.
base::NoDestructor<base::OnceClosure> g_quit_main_message_loop;

const int kDefaultTestWindowWidthDip = 800;
const int kDefaultTestWindowHeightDip = 600;

std::vector<Shell*> Shell::windows_;
base::OnceCallback<void(Shell*)> Shell::shell_created_callback_;

class Shell::DevToolsWebContentsObserver : public WebContentsObserver {
 public:
  DevToolsWebContentsObserver(Shell* shell, WebContents* web_contents)
      : WebContentsObserver(web_contents),
        shell_(shell) {
  }

  // WebContentsObserver
  void WebContentsDestroyed() override {
    shell_->OnDevToolsWebContentsDestroyed();
  }

 private:
  Shell* shell_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWebContentsObserver);
};

#ifdef _WIN32

static int CALLBACK BrowseCallbackProc(HWND hwnd,UINT uMsg, LPARAM lParam, LPARAM lpData)
{

    if(uMsg == BFFM_INITIALIZED)
    {
        std::string tmp = (const char *) lpData;
        std::cout << "path: " << tmp << std::endl;
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }

    return 0;
}


struct handle_data {
    unsigned long process_id;
    HWND window_handle;
};

BOOL is_main_window(HWND handle)
{   
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
    handle_data& data = *(handle_data*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id || !is_main_window(handle))
        return TRUE;
    data.window_handle = handle;
    return FALSE;   
}



HWND find_main_window(unsigned long process_id)
{
    handle_data data = { 0, NULL };
    data.process_id = process_id;
    data.window_handle = 0;
    EnumWindows(enum_windows_callback, (LPARAM)&data);
    return data.window_handle;
}

bool ListDirectoryContents(const wchar_t *sDir, std::vector<FileChooserFileInfoPtr> &files)
{ 
    WIN32_FIND_DATA fdFile; 
    HANDLE hFind = NULL; 

    wchar_t sPath[2048]; 

    //Specify a file mask. *.* = We want everything! 
    wsprintf(sPath, L"%s\\*.*", sDir); 

    if((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE) 
    { 
        //wprintf(L"Path not found: [%s]\n", sDir); 
        return false; 
    } 

    do
    { 
        //Find first file will always return "."
        //    and ".." as the first two directories. 
        if(wcscmp(fdFile.cFileName, L".") != 0
                && wcscmp(fdFile.cFileName, L"..") != 0) 
        { 
            //Build up our file path using the passed in 
            //  [sDir] and the file/foldername we just found: 
            wsprintf(sPath, L"%s\\%s", sDir, fdFile.cFileName); 

            //Is the entity a File or Folder? 
            if(fdFile.dwFileAttributes &FILE_ATTRIBUTE_DIRECTORY) 
            { 
                //wprintf(L"Directory: %s\n", sPath); 
                ListDirectoryContents(sPath, files); //Recursion, I love it! 
            } 
            else{ 
                //wprintf(L"File: %s\n", sPath); 
				
				files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
					blink::mojom::NativeFileInfo::New(base::FilePath(sPath), sPath)));
				
				/*base::FilePath fpath(sPath);
		
				FileChooserFileInfo file_info;
				file_info.file_path = fpath;
				files.push_back(file_info);*/
				
            } 
        }
    } 
    while(FindNextFile(hFind, &fdFile)); //Find the next file. 

    FindClose(hFind); //Always, Always, clean things up! 

    return true; 
} 

void Shell::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
	std::unique_ptr<FileSelectListener> listener,
    const FileChooserParams& params) {
  
  std::vector<FileChooserFileInfoPtr> files;
  
  wchar_t path[MAX_PATH];
  
  if ((ui::SelectFileDialog::Type)params.mode == ui::SelectFileDialog::SELECT_UPLOAD_FOLDER) {
	  
	  
	wchar_t path_param[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, path_param);

	//const wchar_t *path_param = L"C:\\";
	  
	BROWSEINFO bi = { 0 };
    bi.lpszTitle  = L"Browse for folder...";
    bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn       = BrowseCallbackProc;
    bi.lParam     = (LPARAM) path_param;
	bi.hwndOwner  =  find_main_window(GetCurrentProcessId());
	
	
	
    LPITEMIDLIST pidl = SHBrowseForFolder ( &bi );

    if ( pidl != 0 )
    {
        //get the name of the folder and put it in path
        SHGetPathFromIDList ( pidl, path );

        //free memory used
        IMalloc * imalloc = 0;
        if ( SUCCEEDED( SHGetMalloc ( &imalloc )) )
        {
            imalloc->Free ( pidl );
            imalloc->Release ( );
        }
		
		wchar_t buff[MAX_PATH];
		lstrcpy(buff, path);
		lstrcat(buff, L"/ ");
		//base::FilePath fpath(buff);
		
		//FileChooserFileInfoPtr file_info;
		//file_info.file_path = fpath;
		//files.push_back(file_info);
		
		//files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
		//			blink::mojom::NativeFileInfo::New(base::FilePath(buff), buff)));
		
		ListDirectoryContents(path, files);
    }
  }
  else {
		wchar_t fbuff[ MAX_PATH ];

		OPENFILENAME ofn;
		ZeroMemory( &fbuff, sizeof( fbuff ) );
		ZeroMemory( &ofn,      sizeof( ofn ) );
		ofn.lStructSize  = sizeof( ofn );
		ofn.hwndOwner    = find_main_window(GetCurrentProcessId());  // If you have a window to center over, put its HANDLE here
		ofn.lpstrFilter  = L"Any File\0*.*\0";
		ofn.lpstrFile    = fbuff;
		ofn.nMaxFile     = MAX_PATH;
		ofn.lpstrTitle   = L"Select ROMS for upload";
		ofn.Flags        = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER ;
	  
		if (GetOpenFileName( &ofn )) {
			wchar_t * str = ofn.lpstrFile;
			//MessageBox(NULL, str, L"", MB_OK);
			std::wstring directory = str;
			lstrcpy(path, directory.c_str());
			str += ( directory.length() + 1 );
			if (! *str) {
				std::wstring filename = directory;
				//base::FilePath fpath(filename.c_str());
				
				files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
					blink::mojom::NativeFileInfo::New(base::FilePath(filename.c_str()), filename.c_str())));

				//FileChooserFileInfoPtr file_info;
				//file_info.file_path = fpath;
				//files.push_back(file_info);
			}
			while ( *str ) {
				//MessageBox(NULL, str, L"", MB_OK);
				std::wstring filename = directory;
				std::wstring str2 = str;
				filename += L"\\";
				filename += str;
				str += ( str2.length() + 1 );				  
				//base::FilePath fpath(filename.c_str());
				
				
				files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
					blink::mojom::NativeFileInfo::New(base::FilePath(filename.c_str()), filename.c_str())));

				//FileChooserFileInfoPtr file_info;
				//file_info.file_path = fpath;
				//files.push_back(file_info);
			}
		}
  }
  
  
   //base::FilePath _base(
  //render_frame_host->FilesSelectedInChooser(files, params.mode);
   base::FilePath base_dir(path);
  listener->FileSelected(std::move(files), base_dir, (ui::SelectFileDialog::Type)params.mode == ui::SelectFileDialog::SELECT_UPLOAD_FOLDER ? FileChooserParams::Mode::kUploadFolder : FileChooserParams::Mode::kOpenMultiple);
  
}

#elif defined(OS_MACOSX)

void Shell::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
        std::unique_ptr<FileSelectListener> listener,
    const FileChooserParams& params) {

}


#endif

Shell::Shell(std::unique_ptr<WebContents> web_contents,
             bool should_set_delegate)
    : WebContentsObserver(web_contents.get()),
      web_contents_(std::move(web_contents)),
      devtools_frontend_(nullptr),
      is_fullscreen_(false),
      window_(nullptr),
#if defined(OS_MACOSX)
      url_edit_view_(NULL),
#endif
      headless_(false),
      hide_toolbar_(false) {
  if (should_set_delegate)
    web_contents_->SetDelegate(this);

  if (switches::IsRunWebTestsSwitchPresent()) {
    headless_ = !base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableHeadlessMode);
    // Disable occlusion tracking. In a headless shell WebContents would always
    // behave as if they were occluded, i.e. would not render frames and would
    // not receive input events. For non-headless mode we do not want tests
    // running in parallel to trigger occlusion tracking.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableBackgroundingOccludedWindowsForTesting);
  }

  base::CommandLine::ForCurrentProcess()->AppendSwitch("--no-sandbox");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kContentShellHideToolbar);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kContentShellHideToolbar))
    hide_toolbar_ = true;

  windows_.push_back(this);

  if (shell_created_callback_)
    std::move(shell_created_callback_).Run(this);
}

Shell::~Shell() {
  PlatformCleanUp();

  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i] == this) {
      windows_.erase(windows_.begin() + i);
      break;
    }
  }

  // Always destroy WebContents before calling PlatformExit(). WebContents
  // destruction sequence may depend on the resources destroyed in
  // PlatformExit() (e.g. the display::Screen singleton).
  web_contents_->SetDelegate(nullptr);
  web_contents_.reset();

  if (windows_.empty()) {
    if (headless_)
      PlatformExit();
    for (auto it = RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
         it.Advance()) {
      it.GetCurrentValue()->DisableKeepAliveRefCount();
    }
    if (*g_quit_main_message_loop)
      std::move(*g_quit_main_message_loop).Run();
  }
}

Shell* Shell::CreateShell(std::unique_ptr<WebContents> web_contents,
                          const gfx::Size& initial_size,
                          bool should_set_delegate) {
  WebContents* raw_web_contents = web_contents.get();
  Shell* shell = new Shell(std::move(web_contents), should_set_delegate);
  shell->PlatformCreateWindow(initial_size.width(), initial_size.height());

  shell->PlatformSetContents();

  shell->PlatformResizeSubViews();

  // Note: Do not make RenderFrameHost or RenderViewHost specific state changes
  // here, because they will be forgotten after a cross-process navigation. Use
  // RenderFrameCreated or RenderViewCreated instead.
  if (switches::IsRunWebTestsSwitchPresent()) {
    raw_web_contents->GetMutableRendererPrefs()->use_custom_colors = false;
    raw_web_contents->GetRenderViewHost()->SyncRendererPrefs();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceWebRtcIPHandlingPolicy)) {
    raw_web_contents->GetMutableRendererPrefs()->webrtc_ip_handling_policy =
        command_line->GetSwitchValueASCII(
            switches::kForceWebRtcIPHandlingPolicy);
  }

  return shell;
}

void Shell::CloseAllWindows() {
  DevToolsAgentHost::DetachAllClients();
  std::vector<Shell*> open_windows(windows_);
  for (size_t i = 0; i < open_windows.size(); ++i)
    open_windows[i]->Close();

  // Pump the message loop to allow window teardown tasks to run.
  base::RunLoop().RunUntilIdle();

  // If there were no windows open then the message loop quit closure will
  // not have been run.
  if (*g_quit_main_message_loop)
    std::move(*g_quit_main_message_loop).Run();

  PlatformExit();
}

void Shell::SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) {
  *g_quit_main_message_loop = std::move(quit_closure);
}

void Shell::QuitMainMessageLoopForTesting() {
  DCHECK(*g_quit_main_message_loop);
  std::move(*g_quit_main_message_loop).Run();
}

void Shell::SetShellCreatedCallback(
    base::OnceCallback<void(Shell*)> shell_created_callback) {
  DCHECK(!shell_created_callback_);
  shell_created_callback_ = std::move(shell_created_callback);
}

Shell* Shell::FromWebContents(WebContents* web_contents) {
  for (Shell* window : windows_) {
    if (window->web_contents() && window->web_contents() == web_contents) {
      return window;
    }
  }
  return nullptr;
}

void Shell::Initialize() {
  PlatformInitialize(GetShellDefaultSize());
}

gfx::Size Shell::AdjustWindowSize(const gfx::Size& initial_size) {
  if (!initial_size.IsEmpty())
    return initial_size;
  return GetShellDefaultSize();
}

Shell* Shell::CreateNewWindow(BrowserContext* browser_context,
                              const GURL& url,
                              const scoped_refptr<SiteInstance>& site_instance,
                              const gfx::Size& initial_size) {
  WebContents::CreateParams create_params(browser_context, site_instance);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePresentationReceiverForTesting)) {
    create_params.starting_sandbox_flags =
        blink::kPresentationReceiverSandboxFlags;
  }
  create_params.initial_size = AdjustWindowSize(initial_size);
  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(create_params);
  Shell* shell =
      CreateShell(std::move(web_contents), create_params.initial_size,
                  true /* should_set_delegate */);
  	
	base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
	if (command_line->HasSwitch(switches::kEcUrl)) {
		const std::string myurl = command_line->GetSwitchValueASCII(
					  switches::kEcUrl);

		GURL ecurl(myurl.c_str());
		shell->LoadURL(ecurl);
	}
	else {
		GURL ecurl("https://cloudretro.com/");
		shell->LoadURL(ecurl);
	}
	
  return shell;
}

Shell* Shell::CreateNewWindowWithSessionStorageNamespace(
    BrowserContext* browser_context,
    const GURL& url,
    const scoped_refptr<SiteInstance>& site_instance,
    const gfx::Size& initial_size,
    scoped_refptr<SessionStorageNamespace> session_storage_namespace) {
  WebContents::CreateParams create_params(browser_context, site_instance);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePresentationReceiverForTesting)) {
    create_params.starting_sandbox_flags =
        blink::kPresentationReceiverSandboxFlags;
  }
  create_params.initial_size = AdjustWindowSize(initial_size);
  std::map<std::string, scoped_refptr<SessionStorageNamespace>>
      session_storages;
  session_storages[""] = session_storage_namespace;
  std::unique_ptr<WebContents> web_contents =
      WebContents::CreateWithSessionStorage(create_params, session_storages);
  Shell* shell =
      CreateShell(std::move(web_contents), create_params.initial_size,
                  true /* should_set_delegate */);
  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

void Shell::LoadURL(const GURL& url) {
  LoadURLForFrame(
      url, std::string(),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
}

void Shell::LoadURLForFrame(const GURL& url,
                            const std::string& frame_name,
                            ui::PageTransition transition_type) {
  NavigationController::LoadURLParams params(url);
  params.frame_name = frame_name;
  params.transition_type = transition_type;
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void Shell::LoadDataWithBaseURL(const GURL& url, const std::string& data,
    const GURL& base_url) {
  bool load_as_string = false;
  LoadDataWithBaseURLInternal(url, data, base_url, load_as_string);
}

#if defined(OS_ANDROID)
void Shell::LoadDataAsStringWithBaseURL(const GURL& url,
                                        const std::string& data,
                                        const GURL& base_url) {
  bool load_as_string = true;
  LoadDataWithBaseURLInternal(url, data, base_url, load_as_string);
}
#endif

void Shell::LoadDataWithBaseURLInternal(const GURL& url,
                                        const std::string& data,
                                        const GURL& base_url,
                                        bool load_as_string) {
#if !defined(OS_ANDROID)
  DCHECK(!load_as_string);  // Only supported on Android.
#endif

  NavigationController::LoadURLParams params(GURL::EmptyGURL());
  const std::string data_url_header = "data:text/html;charset=utf-8,";
  if (load_as_string) {
    params.url = GURL(data_url_header);
    std::string data_url_as_string = data_url_header + data;
#if defined(OS_ANDROID)
    params.data_url_as_string =
        base::RefCountedString::TakeString(&data_url_as_string);
#endif
  } else {
    params.url = GURL(data_url_header + data);
  }

  params.load_type = NavigationController::LOAD_TYPE_DATA;
  params.base_url_for_data_url = base_url;
  params.virtual_url_for_data_url = url;
  params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void Shell::AddNewContents(WebContents* source,
                           std::unique_ptr<WebContents> new_contents,
                           WindowOpenDisposition disposition,
                           const gfx::Rect& initial_rect,
                           bool user_gesture,
                           bool* was_blocked) {
  WebContents* raw_new_contents = new_contents.get();
  CreateShell(
      std::move(new_contents), AdjustWindowSize(initial_rect.size()),
      !delay_popup_contents_delegate_for_testing_ /* should_set_delegate */);
  if (switches::IsRunWebTestsSwitchPresent())
    SecondaryTestWindowObserver::CreateForWebContents(raw_new_contents);
}

void Shell::GoBackOrForward(int offset) {
  web_contents_->GetController().GoToOffset(offset);
  web_contents_->Focus();
}

void Shell::Reload() {
  web_contents_->GetController().Reload(ReloadType::NORMAL, false);
  web_contents_->Focus();
}

void Shell::ReloadBypassingCache() {
  web_contents_->GetController().Reload(ReloadType::BYPASSING_CACHE, false);
  web_contents_->Focus();
}

void Shell::Stop() {
  web_contents_->Stop();
  web_contents_->Focus();
}

void Shell::UpdateNavigationControls(bool to_different_document) {
  int current_index = web_contents_->GetController().GetCurrentEntryIndex();
  int max_index = web_contents_->GetController().GetEntryCount() - 1;

  PlatformEnableUIControl(BACK_BUTTON, current_index > 0);
  PlatformEnableUIControl(FORWARD_BUTTON, current_index < max_index);
  PlatformEnableUIControl(STOP_BUTTON,
      to_different_document && web_contents_->IsLoading());
}

void Shell::ShowDevTools() {
  if (!devtools_frontend_) {
    devtools_frontend_ = ShellDevToolsFrontend::Show(web_contents());
    devtools_observer_.reset(new DevToolsWebContentsObserver(
        this, devtools_frontend_->frontend_shell()->web_contents()));
  }

  devtools_frontend_->Activate();
  devtools_frontend_->Focus();
}

void Shell::CloseDevTools() {
  if (!devtools_frontend_)
    return;
  devtools_observer_.reset();
  devtools_frontend_->Close();
  devtools_frontend_ = nullptr;
}

gfx::NativeView Shell::GetContentView() {
  if (!web_contents_)
    return nullptr;
  return web_contents_->GetNativeView();
}

WebContents* Shell::OpenURLFromTab(WebContents* source,
                                   const OpenURLParams& params) {
  WebContents* target = nullptr;
  switch (params.disposition) {
    case WindowOpenDisposition::CURRENT_TAB:
      target = source;
      break;

    // Normally, the difference between NEW_POPUP and NEW_WINDOW is that a popup
    // should have no toolbar, no status bar, no menu bar, no scrollbars and be
    // not resizable.  For simplicity and to enable new testing scenarios in
    // content shell and web tests, popups don't get special treatment below
    // (i.e. they will have a toolbar and other things described here).
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::NEW_WINDOW:
    // content_shell doesn't really support tabs, but some web tests use
    // middle click (which translates into kNavigationPolicyNewBackgroundTab),
    // so we treat the cases below just like a NEW_WINDOW disposition.
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::NEW_FOREGROUND_TAB: {
      Shell* new_window =
          Shell::CreateNewWindow(source->GetBrowserContext(),
                                 GURL(),  // Don't load anything just yet.
                                 params.source_site_instance,
                                 gfx::Size());  // Use default size.
      target = new_window->web_contents();
      if (switches::IsRunWebTestsSwitchPresent())
        SecondaryTestWindowObserver::CreateForWebContents(target);
      break;
    }

    // No tabs in content_shell:
    case WindowOpenDisposition::SINGLETON_TAB:
    // No incognito mode in content_shell:
    case WindowOpenDisposition::OFF_THE_RECORD:
    // TODO(lukasza): Investigate if some web tests might need support for
    // SAVE_TO_DISK disposition.  This would probably require that
    // BlinkTestController always sets up and cleans up a temporary directory
    // as the default downloads destinations for the duration of a test.
    case WindowOpenDisposition::SAVE_TO_DISK:
    // Ignoring requests with disposition == IGNORE_ACTION...
    case WindowOpenDisposition::IGNORE_ACTION:
    default:
      return nullptr;
  }

  NavigationController::LoadURLParams load_url_params(params.url);
  load_url_params.initiator_origin = params.initiator_origin;
  load_url_params.source_site_instance = params.source_site_instance;
  load_url_params.transition_type = params.transition;
  load_url_params.frame_tree_node_id = params.frame_tree_node_id;
  load_url_params.referrer = params.referrer;
  load_url_params.redirect_chain = params.redirect_chain;
  load_url_params.extra_headers = params.extra_headers;
  load_url_params.is_renderer_initiated = params.is_renderer_initiated;
  load_url_params.should_replace_current_entry =
      params.should_replace_current_entry;
  load_url_params.blob_url_loader_factory = params.blob_url_loader_factory;
  load_url_params.reload_type = params.reload_type;

  if (params.uses_post) {
    load_url_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;
    load_url_params.post_data = params.post_data;
  }

  target->GetController().LoadURLWithParams(load_url_params);
  return target;
}

void Shell::LoadingStateChanged(WebContents* source,
    bool to_different_document) {
  UpdateNavigationControls(to_different_document);
  PlatformSetIsLoading(source->IsLoading());
}

void Shell::EnterFullscreenModeForTab(
    WebContents* web_contents,
    const GURL& origin,
    const blink::WebFullscreenOptions& options) {
  ToggleFullscreenModeForTab(web_contents, true);
}

void Shell::ExitFullscreenModeForTab(WebContents* web_contents) {
  ToggleFullscreenModeForTab(web_contents, false);
}

void Shell::ToggleFullscreenModeForTab(WebContents* web_contents,
                                       bool enter_fullscreen) {
#if defined(OS_ANDROID)
  PlatformToggleFullscreenModeForTab(web_contents, enter_fullscreen);
#endif
  if (is_fullscreen_ != enter_fullscreen) {
    is_fullscreen_ = enter_fullscreen;
    web_contents->GetRenderViewHost()
        ->GetWidget()
        ->SynchronizeVisualProperties();
  }
}

bool Shell::IsFullscreenForTabOrPending(const WebContents* web_contents) {
#if defined(OS_ANDROID)
  return PlatformIsFullscreenForTabOrPending(web_contents);
#else
  return is_fullscreen_;
#endif
}

blink::WebDisplayMode Shell::GetDisplayMode(const WebContents* web_contents) {
  // TODO: should return blink::WebDisplayModeFullscreen wherever user puts
  // a browser window into fullscreen (not only in case of renderer-initiated
  // fullscreen mode): crbug.com/476874.
  return IsFullscreenForTabOrPending(web_contents)
             ? blink::kWebDisplayModeFullscreen
             : blink::kWebDisplayModeBrowser;
}

void Shell::RequestToLockMouse(WebContents* web_contents,
                               bool user_gesture,
                               bool last_unlocked_by_target) {
  web_contents->GotResponseToLockMouseRequest(true);
}

void Shell::CloseContents(WebContents* source) {
  Close();
}

bool Shell::CanOverscrollContent() {
#if defined(USE_AURA)
  return true;
#else
  return false;
#endif
}

void Shell::DidNavigateMainFramePostCommit(WebContents* web_contents) {
  PlatformSetAddressBarURL(web_contents->GetLastCommittedURL());
}

JavaScriptDialogManager* Shell::GetJavaScriptDialogManager(
    WebContents* source) {
  if (!dialog_manager_) {
    dialog_manager_.reset(switches::IsRunWebTestsSwitchPresent()
                              ? new WebTestJavaScriptDialogManager
                              : new ShellJavaScriptDialogManager);
  }
  return dialog_manager_.get();
}

std::unique_ptr<BluetoothChooser> Shell::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  BlinkTestController* blink_test_controller = BlinkTestController::Get();
  if (blink_test_controller && switches::IsRunWebTestsSwitchPresent())
    return blink_test_controller->RunBluetoothChooser(frame, event_handler);
  return nullptr;
}

std::unique_ptr<BluetoothScanningPrompt> Shell::ShowBluetoothScanningPrompt(
    RenderFrameHost* frame,
    const BluetoothScanningPrompt::EventHandler& event_handler) {
  return std::make_unique<FakeBluetoothScanningPrompt>(event_handler);
}

bool Shell::DidAddMessageToConsole(WebContents* source,
                                   blink::mojom::ConsoleMessageLevel log_level,
                                   const base::string16& message,
                                   int32_t line_no,
                                   const base::string16& source_id) {
  return switches::IsRunWebTestsSwitchPresent();
}

void Shell::PortalWebContentsCreated(WebContents* portal_web_contents) {
  if (switches::IsRunWebTestsSwitchPresent())
    SecondaryTestWindowObserver::CreateForWebContents(portal_web_contents);
}

void Shell::RendererUnresponsive(
    WebContents* source,
    RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  BlinkTestController* blink_test_controller = BlinkTestController::Get();
  if (blink_test_controller && switches::IsRunWebTestsSwitchPresent())
    blink_test_controller->RendererUnresponsive();
}

void Shell::ActivateContents(WebContents* contents) {
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

std::unique_ptr<WebContents> Shell::SwapWebContents(
    WebContents* old_contents,
    std::unique_ptr<WebContents> new_contents,
    bool did_start_load,
    bool did_finish_load) {
  DCHECK_EQ(old_contents, web_contents_.get());
  new_contents->SetDelegate(this);
  web_contents_->SetDelegate(nullptr);
  std::swap(web_contents_, new_contents);
  PlatformSetContents();
  PlatformSetAddressBarURL(web_contents_->GetLastCommittedURL());
  LoadingStateChanged(web_contents_.get(), true);
  return new_contents;
}

bool Shell::ShouldAllowRunningInsecureContent(
    content::WebContents* web_contents,
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  bool allowed_by_test = false;
  BlinkTestController* blink_test_controller = BlinkTestController::Get();
  if (blink_test_controller && switches::IsRunWebTestsSwitchPresent()) {
    const base::DictionaryValue& test_flags =
        blink_test_controller->accumulated_web_test_runtime_flags_changes();
    test_flags.GetBoolean("running_insecure_content_allowed", &allowed_by_test);
  }

  return allowed_per_prefs || allowed_by_test;
}

PictureInPictureResult Shell::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  // During tests, returning success to pretend the window was created and allow
  // tests to run accordingly.
  if (!switches::IsRunWebTestsSwitchPresent())
    return PictureInPictureResult::kNotSupported;
  return PictureInPictureResult::kSuccess;
}

bool Shell::ShouldResumeRequestsForCreatedWindow() {
  return !delay_popup_contents_delegate_for_testing_;
}

gfx::Size Shell::GetShellDefaultSize() {
  static gfx::Size default_shell_size;
  if (!default_shell_size.IsEmpty())
    return default_shell_size;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kContentShellHostWindowSize)) {
    const std::string size_str = command_line->GetSwitchValueASCII(
                  switches::kContentShellHostWindowSize);
    int width, height;
    CHECK_EQ(2, sscanf(size_str.c_str(), "%dx%d", &width, &height));
    default_shell_size = gfx::Size(width, height);
  } else {
    default_shell_size = gfx::Size(
      kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip);
  }
  return default_shell_size;
}

void Shell::TitleWasSet(NavigationEntry* entry) {
  if (entry)
    PlatformSetTitle(entry->GetTitle());
}

void Shell::OnDevToolsWebContentsDestroyed() {
  devtools_observer_.reset();
  devtools_frontend_ = nullptr;
}

}  // namespace content
