# Profile Chromium with Intel&reg; Graphics Performance Analyzers

This page documents how to do graphics profiling on Chromium with **Intel&reg; Graphics Performance Analyzers (GPA)**.

## Introduction to Intel&reg; GPA

Intel&reg; GPA is a toolset for graphics performance analysis and optimizations for graphics-intensive applications.

Currently Intel&reg; GPA works best on Windows when analyzing Chromium. In this document, we will mainly talk about the best practice on how to profile Chromium with latest Intel&reg; GPA on Windows with Intel D3D drivers. We will also keep this document up-to-date when there are any big updates about Intel&reg; GPA in the future.

## Download Intel&reg; GPA

The official link to download Intel&reg; GPA is [here](https://www.intel.com/content/www/us/en/developer/tools/graphics-performance-analyzers/download.html).

## Profile Chromium with Intel&reg; GPA

Currently Intel&reg; GPA **doest't support capturing frame of Chromium with Graphics Monitor**. Instead we can do performance profiling by recording multi-frame streams with the command line tool **gpa-injector.exe** (under the folder 'Streams/' of the install directory of Intel&reg; GPA ).

The steps to record multi-frame streams for D3D11 (for example, WebGL) and D3D12 (for example WebGPU) based graphics applications are different due to some known issues in Intel&reg; GPA. Let's introduce them for you one by one.

For simplicity in this document we always suppose Intel&reg; GPA is installed in its default directory (`C:\Program Files\IntelSWTools\GPA`).

### D3D11-based applications

Here are the steps to profile the WebGL application [Aquarium](https://webglsamples.org/aquarium/aquarium.html) with Intel&reg; GPA.

1. From a command line window, start recording streams by executing below commands. We will see Chrome is launched and the given url is opened.
```
cd C:\Program Files\IntelSWTools\GPA\Streams

gpa-injector.exe --injection-mode 1 -t "%LOCALAPPDATA%\Google\Chrome SxS\Application\chrome.exe" -L capture -L hud-layer -- --no-sandbox https://webglsamples.org/aquarium/aquarium.html
```

- `-t` specifies the binary file of the graphics application.
- `-L hud-layer` (optional) enables the HUD of Intel&reg; GPA, where we can see some statistics (for example, the number of frames we have collected) of Intel&reg; GPA.
- `--` specifies the command line parameters that will be used when the graphics application is launched. Note that `--no-sandbox` is required when we profile Chrome with Intel&reg; GPA.

2. Close Chrome to stop recording the stream after we have collected enough frames.

3. Open **Graphics Frame Analyzer** (another tool in Intel&reg; GPA), and we can see all the streamfiles we have collected before.

4. Open the stream file we'd like to analyze, and we can see all the frames included in the stream file.

5. Open the frame we are interested in and wait for the completion of stream play back, then we can choose the draw call we'd like to analyze and see all the statistics collected by Intel&reg; GPA.


### D3D12-based applications

Here are the steps to profile the WebGPU application [ComputeBoids](https://austin-eng.com/webgpu-samples/samples/computeBoids) with Intel&reg; GPA.

1. From a command line window, start recording streams by executing below commands. we will see Chrome is launched and the given url is opened.
```
cd C:\Program Files\IntelSWTools\GPA\Streams

gpa-injector.exe --injection-mode 1 --hook-d3d11on12 -t "C:\Program Files\Google\Chrome\Application\chrome.exe" -L capture -L hud-layer -- --enable-unsafe-webgpu --no-sandbox --enable-dawn-features=emit_hlsl_debug_symbols,use_dxc,disable_symbol_renaming https://austin-eng.com/webgpu-samples/samples/computeBoids
```

- `--hook-d3d11on12` is required to profile WebGPU applications.

2. Close Chrome to stop recording the stream after we have collected enough frames.

3. Open the stream file we'd like to analyze with **Graphics Frame Analyzer**.

4. Enable "`Multi-Frame Profiling View (DirectX 11 Tech Preview)`" in "`General Settings`" (a button on top right of the UI).

5. Click the "`Direct Queue 1`" tab (on the left of the UI), and open a call of "`ExecuteCommandlists`" we are interested in.

6. Wait for the completion of stream play back, then we can choose the draw call or the dispatch call we'd like to analyze and see all the statistics collected by Intel&reg; GPA.

## Known Issues

Currently [Intel&reg; GPA Shader Profiler](https://www.intel.com/content/www/us/en/develop/documentation/gpa-user-guide/top/analyze-desktop-apps/optimize-specific-domains/optimize-shaders.html) doesn't work with Chromium.

## More Information

Please visit [the official website of Intel&reg; GPA](https://www.intel.com/content/www/us/en/developer/tools/graphics-performance-analyzers/overview.html) for more information.
