# Chrome Screen AI Library

## Purpose
Chrome Screen AI library provides two on-device functionalities for Chrome and
ChromeOS:
* **Main Content Extraction:** Intelligently isolates the main content of a web
   page, improving its readability by stripping distracting elements (based on
   the accessibility tree).
* **Optical Character Recognition:** Extracts text from image.

These functionalities are entirely on device and do not send any data to
network or store on disk.

## Development and Deployment
* **Source:** Developed within Google's internal source code repository
   (google3).
* **Platforms:** Built for ChromeOS, Linux, Mac, and Windows.
* **Distribution:**
  * **ChromeOS:** Distributed via DLC (Dynamic Link Component).
  * **Linux, Mac, Windows:** Delivered on-demand through the component updater.

## How to Use
See: `chrome/browser/screen_ai/README.md`

## Bugs Component:
  Chromium > UI > Accessibility > MachineIntelligence
