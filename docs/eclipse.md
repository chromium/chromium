# Eclipse Configuration for Android
[TOC]

Instructions on using **Android Studio** are [here](android_studio.md).

*** note
This documentation was written for Eclipse 4.5 though other versions of Eclipse
should work similarly.
***

## One time Eclipse Configuration
This section contains set up steps for the very first time you start Eclipse.
You should only need to go through these once, even if you switch workspaces.

 * Starting Eclipse. It can be started a couple of ways:
    * From Ubuntu main menu, select Applications > Programming > Eclipse 4.5
    * From the command line, type eclipse45
 * Pick a workspace somewhere in your hard drive.
 * Install CDT - C/C++ Development Tools
    * Select Help > Install New Software... from the main menu
    * For Work with, select mars - http://download.eclipse.org/releases/mars
      (or any other mirror that seems appropriate)
    * Check Mobile and Device Development > C/C++ Remote Launch
    * Check Programming Languages > C/C++ Development Tools
    * Click through Next, Finish, etc to complete the wizard.
    * If you get errors about a missing dependency on org.eclipse.rse.ui for the
      "C++ Remote Launch" install, add an available update path of
      http://download.eclipse.org/dsdp/tm/updates/3.2
    * Click Restart Now when prompted to restart Eclipse
 * Memory
    * Close Eclipse
    * Add the following lines to `~/.eclipse/init.sh`:

      ```shell
      ECLIPSE_MEM_START=1024m
      ECLIPSE_MEM_MAX=8192m
      ```

## General Workspace Configuration
These are settings that apply to all projects in your workspace. All the
settings below are inside Window > Preferences.

 * Android formatting
    * Download [android-formatting.xml](https://raw.githubusercontent.com/android/platform_development/master/ide/eclipse/android-formatting.xml)
    * Select Java > Code Style > Formatter from the tree on the left
    * Click Import...
    * Select the android-formatting.xml file
    * Make sure Android is set as the Active Profile
 * Java import order
    * Select Java > Code Style > Organize Imports from the tree on the left
    * Click Import...
    * Select the `<project root>/tools/android/eclipse/android.importorder` file
 * Disable automatic refresh. Otherwise, Eclipse will constantly try to refresh
   your projects (which can be slow).
    * Select General > Workspace from the tree on the left.
    * Uncheck Refresh using native hooks or polling (if present)
    * Select General > Startup and Shutdown from the tree on the left.
    * Uncheck Refresh workspace on startup (if present)
 * Disable build before launching
    * Select Run/Debug > Launching
    * Uncheck Build (if required) before launching
 * Tab ordering
    * If you prefer ordering your tabs by most recently used, go to General >
      Appearance and check Show most recently used tabs
 * Autocomplete
    * Select Java > Editor > Content Assist
    * Check Enable auto activation
    * Change Auto activation triggers for Java: to
      `._abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ`
 * Line Length
    * If you want to change the line length indicator, go to General > Editors >
      Text Editors
    * Check Show print margin and change the Print margin column: to 100.

## Project Configuration

### Create the project

 * Select File > New > Project... from the main menu.
 * Select C/C++ > C++ from the project tree and click Next.
    * Note: not "Makefile Project with Existing Code", even though that sounds
      sensible).
 * For Project name, use something meaningful to you. For example
   "chrome-android”
 * Uncheck Use default location. Click Browse... and select the src directory of
   your Chromium gclient checkout
 * For Project type, use Makefile project > Empty Project
 * For Toolchains use -- Other Toolchain --
 * Click Next
 * Disable the default CDT builder
    * Click Advanced Settings...
    * Select Builders from the tree on the left
    * Uncheck CDT Builder
    * Click OK if a dialog appears warning you that this is an
      'advanced feature'
    * Click OK to close the project properties dialog and return to the project
      creation wizard
 * Click Finish to create the project

### Configure the project

 * Right click on the project and select Properties
 * Exclude Resources (OPTIONAL). This can speed Eclipse up a bit and may make
   the indexer happier.
    * Select Resources > Resource Filters
    * Click Add...
    * Select Exclude all, Select Folders, and check All children (recursive)
    * Enter .git as the name
    * Click OK
    * Click Apply to commit the changes
 * C/C++ Indexer (deprecated, seems to be done by default)</span>
    * Select C/C++ General > Indexer from the tree on the left
    * Click Restore Defaults
    * Check Enable project specific settings
    * Uncheck Index source files not included in the build
    * Uncheck Allow heuristic resolution of includes
    * Click Apply to commit the changes
 * Java
    * Create a link from `<project root>/.classpath` to
      `<project root>/tools/android/eclipse/.classpath`:
      ```shell
      ln -s tools/android/eclipse/.classpath .classpath
      ```

 * Edit `<project root>/.project` as follows to make your project a
   Java project:
    * Add the following lines inside `<buildSpec>`:
      ```xml
      <buildCommand>
        <name>org.eclipse.jdt.core.javabuilder</name>
        <arguments></arguments>
      </buildCommand>
      ```
    * Add the following line inside `<natures>`:
      ```xml
      <nature>org.eclipse.jdt.core.javanature</nature>
      ```

### Run Robolectric JUnit tests

 * Prerequisite: Install a Java 8 JRE and make sure it's available in Eclipse
 * Create a new JUnit test target:
    * Run > Run Configurations > New launch configuration > JUnit
    * Using Android JUnit test launcher
    * Test tab:
       * Run a single test or all tests in the package you want
    * Arguments tab:
       * VM arguments:

         ```
         -Drobolectric.dependency.dir=out/Debug/lib.java/third_party/robolectric
         ```

    * Classpath tab:
       * Bootstrap Entries:
          * Advanced... > Add Library > JRE System Library
          * Select a Java 8 JRE
       * User Entries:
          * Add JARs...
          * Select the following JAR file from `third_party/robolectric/lib`:
             * `android-all-5.0.0_r2-robolectric-1.jar`
    * JRE tab:
       * Execution environment: Select the Java 8 JRE
 * Run or Debug the launch configuration

