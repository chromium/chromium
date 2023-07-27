# iOS Documents Directory Statistics Viewer

This tool allows interactive analysis of the statistics file created by
`documents_statistics::DumpSandboxFileStatistics`.

## Report Generation

To generate a report, enable the switch within the system Settings app.
`Settings > Chrome Canary > Experimental Settings > Dump Sandbox File Statistics
to Documents` then re-launch Chrome. Wait at least 30 seconds with Chrome in the
foreground while the report is created. Once complete, it will be placed within
the Application's Documents directory inside the `sandboxFileStats` directory.

NOTE: This option is intentionally NOT generally available on Stable. It
requires a build with the `ios_enable_sandbox_dump` arg enabled, which is
usually enabled for Canary/Dev builds.

## Obtaining the Report

In order to analyze the report, you need to copy it from your device to your
computer (where this directory resides locally) using either of the following
methods:

*   You can access this file from the iOS Files app by navigating to `Files >
    Browse > On My iPhone > Chrome Canary > sandboxFileStats`. Then use any of
    the share sheet options to send the file via a means accessible on your
    computer.
*   Connect your iOS device to the computer via USB. (Be sure to "Trust" the
    computer if prompted.) Navigate to Finder and select your iOS device from
    the sidebar. ("Trust" the device in Finder if necessary.) Select the "Files"
    tab, then expand the Chrome Canary app item. Drag the `sandboxFileStats`
    directory onto your computer in a discoverable location.

## Report Analysis

To inspect a report, open `./viewer.html` in your desktop browser.

## Removing Downloaded File names from Report

The generated report contains the real file and directory names from the
application sandbox. If you'd like to share this file with someone else, you can
use the `cleanSandboxFileStats.py` script to replace the real names of downloads
with a placeholder value.

### Example Usage

For example, running this command:

```
cleanSandboxFileStats.py sandboxFileStats/file.json
```

will generate a file at `sandboxFileStats/file_clean.json` with the names of all
files and directories under the path which stores user downloads replaced with
`##DOWNLOADED_ITEM##`.

## Making Changes

The viewer only uses the files `viewer.html`, `viewer.css`, and `tsc/viewer.js`.
Do not edit the JavaScript file directly, but rather edit `viewer.ts` and then
generate the JavaScript file by running `./compile_typescript.py` from this
directory. Commit the generated file along with the modified JavaScript source
to allow for immediate use of this viewer without the need for the user to
compile the TypeScript.
