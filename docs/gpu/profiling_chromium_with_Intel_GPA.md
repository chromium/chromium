# Profile Chromium with Intel&reg; Graphics Performance Analyzers

This page documents how to do graphics profiling on Chromium with **Intel&reg; Graphics Performance Analyzers (GPA)**.

## Introduction to Intel&reg; GPA

Intel&reg; GPA is a toolset for graphics performance analysis and optimizations for graphics-intensive applications.

Currently Intel&reg; GPA works best on Windows when analyzing Chromium. In this document, we will mainly talk about the best practice on how to profile Chromium with latest Intel&reg; GPA on Windows with Intel&reg D3D drivers. All the methods mentioned in this document should work with the latest version of Intel&reg; GPA (2023 R4) and the latest Windows Intel&reg; GPU driver (31.0.101.5330). We will also keep this document up-to-date when there are any big updates about Intel&reg; GPA in the future.

## Download Intel&reg; GPA

The official link to download Intel&reg; GPA is [here](https://www.intel.com/content/www/us/en/developer/tools/graphics-performance-analyzers/download.html).

## Profile Chromium with Intel&reg; GPA

Currently Intel&reg; GPA **doesn't support capturing frame of Chromium with Graphics Monitor**. Instead we can do performance profiling by recording multi-frame streams with the command line tool **gpa-injector.exe** (under the folder 'Streams/' of the install directory of Intel&reg; GPA).

The steps to record multi-frame streams for D3D11 (for example, WebGL) and D3D12 (for example WebGPU) based graphics applications are different due to some known issues in Intel&reg; GPA. Let's introduce them for you one by one.

For simplicity in this document we always suppose Intel&reg; GPA is installed in its default directory (`C:\Program Files\IntelSWTools\GPA`).

### D3D11-based applications

Here are the steps to profile the WebGL application [Aquarium](https://webglsamples.org/aquarium/aquarium.html) with Intel&reg; GPA.

1. From a command line window, start recording streams by executing below commands. We will see Chrome is launched and the given url is opened.
```
cd C:\Program Files\IntelSWTools\GPA\Streams

gpa-injector.exe --injection-mode 1 -t "%LOCALAPPDATA%\Google\Chrome SxS\Application\chrome.exe" -L capture -L hud-layer -- --no-sandbox --disable_direct_composition=1 https://webglsamples.org/aquarium/aquarium.html
```

Note that
- `-t` specifies the binary file of the graphics application.
- `-L hud-layer` (optional) enables the HUD of Intel&reg; GPA, where we can see some statistics (for example, the number of frames we have collected) of Intel&reg; GPA.
- `--` specifies the command line parameters that will be used when the graphics application is launched. Currently both `--no-sandbox` and `--disable_direct_composition=1` are required for the latest version of Intel&reg; GPA to work with Chromium.

2. Close Chrome to stop recording the stream after we have collected enough frames.

3. Open **Graphics Frame Analyzer** (another tool in Intel&reg; GPA), and we can see all the stream files we have collected before. To enable shader profiler we should also enable the option `Enable Shader Profiler` on `General Settings` of Graphics Frame Analyzer.

4. Open the stream file we'd like to analyze, and we can see all the frames included in the stream file.

5. Open the frame we are interested in and wait for the completion of stream play back, then we can choose the draw call we'd like to analyze and see all the statistics collected by Intel&reg; GPA.


### D3D12-based applications

Here are the steps to profile the WebGPU application [ComputeBoids](https://austin-eng.com/webgpu-samples/samples/computeBoids) with Intel&reg; GPA.

1. From a command line window, start recording streams by executing below commands. we will see Chrome is launched and the given url is opened.
```
cd C:\Program Files\IntelSWTools\GPA\Streams

gpa-injector.exe --injection-mode 1 --hook-d3d11on12 -t "C:\Program Files\Google\Chrome\Application\chrome.exe" -L capture -L hud-layer -- --enable-unsafe-webgpu --no-sandbox --enable-dawn-features=emit_hlsl_debug_symbols,use_dxc,disable_symbol_renaming --disable_direct_composition=1 https://austin-eng.com/webgpu-samples/samples/computeBoids
```

Note that
- `--hook-d3d11on12` is required to profile WebGPU applications.
- `--no-sandbox` and `--disable_direct_composition=1` are both required for the latest version of Intel&reg; GPA to work with Chromium.

2. Close Chrome to stop recording the stream after we have collected enough frames.

3. Open the stream file we'd like to analyze with **Graphics Frame Analyzer**, and we can see all the stream files we have collected before. To enable shader profiler we should also enable the option `Enable Shader Profiler` on `General Settings` of Graphics Frame Analyzer.

4. Click the "`Direct Queue 1`" tab (on the left of the UI), and open a call of "`ExecuteCommandlists`" we are interested in.

5. Wait for the completion of stream play back, then we can choose the draw call or the dispatch call we'd like to analyze and see all the statistics collected by Intel&reg; GPA.

## More Information

Please visit [the official website of Intel&reg; GPA](https://www.intel.com/content/www/us/en/developer/tools/graphics-performance-analyzers/overview.html) for more information.
