# High-level overview of Save-Page-As code

This document describes code under `//content/browser/download`
restricting the scope only to code handling Save-Page-As functionality
(i.e. leaving out other downloads-related code).
This document focuses on high-level overview and aspects of the code that
span multiple compilation units (hoping that individual compilation units
are described by their code comments or by their code structure).

## Classes overview

* SavePackage class
    * coordinates overall save-page-as request
    * created and owned by `WebContents`
      (ref-counted today, but it is unnecessary - see https://crbug.com/596953)
    * UI-thread object

* SaveFileCreateInfo::SaveFileSource enum
    * classifies `SaveItem` and `SaveFile` processing into 2 flavours:
        * `SAVE_FILE_FROM_NET` (see `SaveFileResourceHandler`)
        * `SAVE_FILE_FROM_DOM` (see "Complete HTML" section below)

* SaveItem class
    * tracks saving a single file
    * created and owned by `SavePackage`
    * UI-thread object

* SaveFileManager class
    * coordinates between the download sequence and the UI thread
        * Gets requests from `SavePackage` and communicates results back to
          `SavePackage` on the UI thread.
        * Shephards data (received from the network OR from DOM) into
          the download sequence - via `SaveFileManager::UpdateSaveProgress`
    * created and owned by `BrowserMainLoop`
      (ref-counted today, but it is unnecessary - see https://crbug.com/596953)
    * The global instance can be retrieved by the Get method.

* SaveFile class
    * tracks saving a single file
    * created and owned by `SaveFileManager`
    * download sequence object

* SaveFileCreateInfo POD struct
    * short-lived object holding data passed to callbacks handling start of
      saving a file.

* MHTMLGenerationManager class
    * singleton that manages progress of jobs responsible for saving individual
      MHTML files (represented by `MHTMLGenerationManager::Job`).


## Overview of the processing flow

Save-Page-As flow starts with `WebContents::OnSavePage`.
The flow is different depending on the save format chosen by the user
(each flow is described in a separate section below).

### Complete HTML

Very high-level flow of saving a page as "Complete HTML":

* Step 1: `SavePackage` asks all frames for "savable resources"
          and creates `SaveItem` for each of files that need to be saved

* Step 2: `SavePackage` first processes `SAVE_FILE_FROM_NET`
          `SaveItem`s and asks `SaveFileManager` to save
          them.

* Step 3: `SavePackage` handles remaining `SAVE_FILE_FROM_DOM` `SaveItem`s and
          asks each frame to serialize its DOM/HTML (each frame gets from
          `SavePackage` a map covering local paths that need to be referenced by
          the frame).  Responses from frames get forwarded to `SaveFileManager`
          to be written to disk.


### MHTML

Very high-level flow of saving a page as MHTML:

* Step 1: `WebContents::GenerateMHTML` is called by either `SavePackage` (for
          Save-Page-As UI) or Extensions (via `chrome.pageCapture` extensions
          API) or by an embedder of `WebContents` (since this is public API of
          //content).

* Step 2: `MHTMLGenerationManager` creates a new instance of
          `MHTMLGenerationManager::Job` that coordinates generation of
          the MHTML file by sequentially (one-at-a-time) asking each
          frame to write its portion of MHTML to a file handle.  Other
          classes (i.e. `SavePackage` and/or `SaveFileManager`) are not
          used at this step at all.

* Step 3: When done `MHTMLGenerationManager` destroys
          `MHTMLGenerationManager::Job` instance and calls a completion
          callback which in case of Save-Page-As will end up in
          `SavePackage::OnMHTMLGenerated`.

Note: MHTML format is by default disabled in Save-Page-As UI on Windows, MacOS
and Linux (it is the default on Chrome OS), but for testing this can be easily
changed using `--save-page-as-mhtml` command line switch.


### HTML Only

Very high-level flow of saving a page as "HTML Only":

* `SavePackage` creates only a single `SaveItem` (always `SAVE_FILE_FROM_NET`)
  and asks `SaveFileManager` to process it
  (as in the Complete HTML individual SaveItem handling above.).


## Other relevant code

Pointers to related code outside of `//content/browser/download`:

* End-to-end tests:
    * `//chrome/browser/download/save_page_browsertest.cc`
    * `//chrome/test/data/save_page/...`

* Other tests:
    * `//content/browser/download/*test*.cc`
    * `//content/renderer/dom_serializer_browsertest.cc` - single process... :-/

* Elsewhere in `//content`:
    * `//content/renderer/savable_resources...`

* Blink:
    * `//third_party/blink/public/web/web_frame_serializer...`
    * `//third_party/blink/renderere/core/frame/web_frame_serializer_impl...`
      (used for Complete HTML today;  should use `FrameSerializer` instead in
      the long-term - see https://crbug.com/328354).
    * `//third_party/blink/renderer/core/frame/frame_serializer...`
      (used for MHTML today)
    * `//third_party/blink/renderer/platform/mhtml/mhtml_archive...`

