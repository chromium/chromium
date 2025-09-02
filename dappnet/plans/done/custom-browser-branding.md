# Customizing Chrome Browser Icon and Name for Dappnet

This guide explains how to rebrand Chromium with a custom icon and browser name for Dappnet.

## Overview

Chromium's branding can be customized by modifying specific files in the source tree. This involves changing icons, product names, and build configurations.

## Quick Implementation

A Python script has been created to automate the icon replacement process:

```bash
# Run the icon generation script
python3 /home/liam/chromium/src/dappnet/generate_chrome_icons.py
```

This script automatically:
- Generates all required icon sizes from the Dappnet icon resources
- Copies icons to appropriate Chrome theme directories
- Handles Windows (.ico), macOS (.icns), and Linux (.png) formats

## 1. Changing the Browser Icon (COMPLETED)

### Icon Source Files
The Dappnet icons are located in:
```
dappnet/dappnet/desktop-app/dist-resources/
├── icon.png (512x512 source)
├── icon.ico (Windows)
└── icon.icns (macOS)
```

### Automated Icon Deployment
The following icons have been automatically generated and deployed:

#### Windows Icons ✓
- `chrome/app/theme/chromium/chromium.ico` - Main application icon
- `chrome/installer/setup/setup.ico` - Installer icon

#### Linux Icons ✓
- `chrome/app/theme/chromium/product_logo_16.png` - 16x16 icon
- `chrome/app/theme/chromium/product_logo_32.png` - 32x32 icon
- `chrome/app/theme/chromium/product_logo_48.png` - 48x48 icon
- `chrome/app/theme/chromium/product_logo_64.png` - 64x64 icon
- `chrome/app/theme/chromium/product_logo_128.png` - 128x128 icon
- `chrome/app/theme/chromium/product_logo_256.png` - 256x256 icon
- `chrome/app/theme/chromium/product_logo_name.png` - Desktop entry icon

#### macOS Icons ✓
- `chrome/app/theme/chromium/mac/app.icns` - macOS application icon

## 2. Changing the Browser Name

### Update Build Configuration

1. Edit `chrome/app/chromium_strings.grd`:
   ```xml
   <message name="IDS_PRODUCT_NAME" desc="The Chrome application name">
     Dappnet
   </message>
   ```

2. Modify `chrome/common/chrome_constants.cc`:
   ```cpp
   const char kProductName[] = "Dappnet";
   const char kBrowserProcessExecutableName[] = "Dappnet";
   ```

3. Update `chrome/installer/linux/common/chromium-browser/chromium-browser.info`:
   ```
   PACKAGE="Dappnet"
   PROGNAME="Dappnet"
   MENUNAME="Dappnet"
   ```

### Update Branding Files

1. Create or modify `chrome/app/theme/chromium/BRANDING`:
   ```
   COMPANY_FULLNAME=Dappnet
   COMPANY_SHORTNAME=Dappnet
   PRODUCT_FULLNAME=Dappnet
   PRODUCT_SHORTNAME=Dappnet
   PRODUCT_INSTALLER_FULLNAME=Dappnet Installer
   PRODUCT_INSTALLER_SHORTNAME=Dappnet Installer
   ```

2. For official branding, you may need to create a new branding directory:
   ```
   chrome/app/theme/Dappnet/
   ```
   And copy/modify all branding assets there.

## 3. Build Configuration Changes

### GN Build Arguments

Add to your `args.gn`:
```
chrome_branding = "Chromium"  # or create custom "Dappnet" branding
enable_chrome_browser_cloud_management = false
```

### Custom Branding Option

For a complete rebrand, create a new branding configuration:

1. Create `chrome/app/theme/Dappnet/` directory structure
2. Copy all files from `chrome/app/theme/chromium/` to your new directory
3. Modify build files to recognize "Dappnet" as a branding option
4. Update `chrome/BUILD.gn` to include your branding option

## 4. Additional Customizations

### User Agent String
Modify `chrome/common/chrome_content_client.cc`:
```cpp
std::string product = "Dappnet/" + version_info::GetVersionNumber();
```

### About Dialog
Update `chrome/browser/ui/webui/about_ui.cc` for custom about page branding.

### Window Title
The window title format can be customized in:
```
chrome/browser/ui/views/frame/browser_view.cc
```

## 5. Implementation Status

### ✅ Completed Steps
1. **Icon Resources Deployed** - All Dappnet icons have been copied to Chrome theme directories
   - Run `python3 /home/liam/chromium/src/dappnet/generate_chrome_icons.py` to re-deploy if needed
2. **Product Name Strings Updated** - Modified `chrome/app/chromium_strings.grd` to use "Dappnet"
3. **Chrome Constants Updated** - Product name references updated (uses build macros)
4. **Linux Installer Configuration** - Updated `chrome/installer/linux/common/chromium-browser.info`
5. **BRANDING Files Updated** - Modified `chrome/app/theme/chromium/BRANDING` with Dappnet branding

### ⏳ Next Step
1. **Build with New Branding** - Run the build commands below

## 6. Summary of Modified Files

The following files have been modified for Dappnet branding:

### Icons (Generated/Copied)
- ✅ `chrome/app/theme/chromium/product_logo_16.png`
- ✅ `chrome/app/theme/chromium/product_logo_32.png`
- ✅ `chrome/app/theme/chromium/product_logo_48.png`
- ✅ `chrome/app/theme/chromium/product_logo_64.png`
- ✅ `chrome/app/theme/chromium/product_logo_128.png`
- ✅ `chrome/app/theme/chromium/product_logo_256.png`
- ✅ `chrome/app/theme/chromium/product_logo_name.png`
- ✅ `chrome/app/theme/chromium/chromium.ico`
- ✅ `chrome/app/theme/chromium/mac/app.icns`
- ✅ `chrome/installer/setup/setup.ico`

### Configuration Files (Modified)
- ✅ `chrome/app/chromium_strings.grd` - Product name strings
- ✅ `chrome/app/theme/chromium/BRANDING` - Branding configuration
- ✅ `chrome/installer/linux/common/chromium-browser.info` - Linux installer config

### Helper Scripts (Created)
- ✅ `dappnet/generate_chrome_icons.py` - Automated icon deployment script

## 7. Building with Custom Branding

After completing all branding changes:

```bash
# Clean previous builds
gn clean out/Default

# Regenerate build files
gn gen out/Default

# Build Chrome with new branding
autoninja -C out/Default chrome
```

## Important Notes

- Always backup original files before modification
- Some branding changes require a clean rebuild
- Test thoroughly on all target platforms
- Consider trademark and licensing implications
- For production builds, create a proper branding configuration rather than modifying Chromium directly

## Testing

After building:
1. Verify the executable name is correct
2. Check that icons appear correctly in:
   - Taskbar/dock
   - Window decorations
   - About dialog
   - File associations
3. Confirm product name appears correctly in:
   - Title bars
   - About dialog
   - Task manager
   - System dialogs

## Troubleshooting

- If icons don't update, clear icon caches:
  - Windows: Delete IconCache.db
  - Linux: Update desktop database with `update-desktop-database`
  - macOS: Clear icon cache with `sudo find /private/var/folders/ -name com.apple.iconservices -exec rm -rf {} \;`

- If name changes don't appear, ensure all string resources are rebuilt:
  ```bash
  autoninja -C out/Default chrome/app:generated_resources_grd
  ```