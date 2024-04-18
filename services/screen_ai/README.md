# Chrome Screen AI Library

## Purpose
Chrome Screen AI library provides two on-device functionalities for Chrome and ChromeOS:
 - **Main Content Extraction:** Intelligently isolates the core content of a web
   page, improving its readability by stripping distracting elements (based on
   the accessibility tree).
 - **Optical Character Recognition:** Analyzes images to extract text.

## Development and Deployment
 - **Source:** Developed within Google's internal source code repository (google3).
 - **Platforms:** Built for ChromeOS, Linux, Mac, and Windows.
 - **Distribution:**
   - **ChromeOS:** Distributed via DLC (Dynamic Link Component).
   - **Linux, Mac, Windows:** Delivered on-demand through the component updater.

## How to Use for OCR
 -  when all use cases are covered.
 - ScreenAI service is downloaded and initialized on demand, and stays on disk
   for 30 days after the last use.
 - If your use case allows, before promising the availability to user, call
   `screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(browser_context)::GetServiceStateAsync`
   to trigger the service initialization and receive the result in a callback when download and
   initialization (if needed) are finished.
 - Create a `screen_ai::OpticalCharacterRecognizer` object.
 - If you have not used `IsReadyAsync`, wait until
   `screen_ai::OpticalCharacterRecognizer::IsReady` returns true.
 - Call `OpticalCharacterRecognizer::PerformOCR` with an image and get the OCR
   results. If service is not yet initialized, this call will return empty.
 - TODO(crbug.com/327181467): Add interfaces and instructions for calls from
   renderer process.

## How to use for Main Content Extraction (to be updated as OCR)
  - **Library Availability Check:** Since the library isn't pre-installed with
    Chrome, confirm its presence before enabling Screen AI features by using
    `ScreenAIServiceRouter::GetServiceStateAsync` to query the library's
    download and readiness status.
  - **Interface Creation:** Once the library is confirmed available, create a
    mojo interface to access the desired functionality. See the following as an
    example, focusing on the `ExtractMainContent` call:
    `chrome/renderer/accessibility/ax_tree_distiller.cc`

## Bugs Component:
  Chromium > UI > Accessibility > MachineIntelligence
