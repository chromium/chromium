# CLion Dev

CLion is an IDE

Prerequisite:
[Checking out and building the chromium code base](README.md#Checking-Out-and-Building)

[TOC]

## Setting up CLion

1. Install CLion.
    - Googlers only: See
      [go/intellij-getting-started](https://goto.google.com/intellij-getting-started)
      for installation and license authentication instructions.

1. Run CLion

1. Increase CLion's memory allocation
    - This step will help performance with large projects
    1. Option 1
        1. At the startup dialogue, in the bottom right corner, click
           `Configure`.
        1. Setup `Edit Custom VM Options`:
            ```
            -Xss2m
            -Xms1g
            -Xmx5g
            ```
        1. Setup `Edit Custom Properties`:
            ```
            idea.max.intellisense.filesize=12500
            ```
    1. Option 2; 2017 and prior versions may not include the options to setup your `VM Options` and `Properties` in the `configure` menu. Instead:
        1. `Create New Project`
        1. `Help` > `Edit Custom VM Options`
        1. `Help` > `Edit Custom Properties`

## Chromium in CLion

1. Import project
    - At the startup dialog, select `Import Project` and select your `chromium`
      directory; this should be the parent directory to `src`. Selecting `src`
      instead would result in a bunch of CLion IDE files appearing in your
      repository.

1. Modify the `CMakeLists.txt` file
    1. Open the `CMakeLists.txt` file
    1. Add the following to the top
        ```
        set(CMAKE_BUILD_TYPE Debug)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
        ```
    1. Remove any other `include_directories` the file contains. The beginning
       of the file should now look like:
        ```
        cmake_minimum_required(VERSION 3.10)
        project(chromium)

        set(CMAKE_CXX_STANDARD 11)

        set(CMAKE_BUILD_TYPE Debug)

        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

        add_executable(chromium
            ...)
        ```

## Building, Running, and Debugging within CLion

1. `Run` > `Edit Configurations`
1.  Click `+` in the top left and select `Application`
1. Setup:
    ```
    Target: All targets
    Executable: src/out/Defaults/chrome
    Program arguments (optional): --disable-seccomp-sandbox --single-process
    Working directory: .../chromium/src/out/Default
    ```
1. Click `+` next to the `Before launch` section and select `Run External tool`
1. In the dialog that appears, click `+` and setup:
    ```
    Program: .../depot_tools/ninja
    Arguments: -C out/Default -j 1000 chrome
    Working directory: .../chromium/src
    ```
1. Click `OK` to close all three dialogs
1. `Run` > `Run` or `Run` > `Debug`

## Note on installing CLion on Linux

For some reason, when installing 2018.1 through a package manager, it did not create a desktop entry when I tried it. If you experience this as well:

1. Option 1
    1. `cd /usr/share/applications/`
    1. `touch clion-2018-1.desktop`
    1. open `clion-2018-1.desktop` and insert:
        ```
        [Desktop Entry]
        Name=CLion 2018.1
        Exec=/opt/clion-2018.1/bin/clion.sh %u
        Icon=/opt/clion-2018.1/bin/clion.svg
        Terminal=false
        Type=Application
        Categories=Development;IDE;Java;
        StartupWMClass=jetbrains-clion
        X-Desktop-File-Install-Version=0.23
        ```
1. Option 2
    1. Run CLion through the terminal `/opt/clion-2018.1/bin/clion.sh`
    1. At the startup dialogue, in the bottom right corner, click `configure`
    1. Click `Create Desktop Entry`

## Optional Performance Steps

### Mark directories as `Library Files`

To speed up CLion, you may optionally mark directories such as `src/third_party` as `Library Files`
1. Open the `Project` navigation (default `Alt 1`)
1. Right click the directory > `Mark directory as` > `Library Files`
1. See `https://blog.jetbrains.com/clion/2015/12/mark-dir-as/` for more details
