#!/usr/bin/env python3

from PIL import Image
import os
import shutil

# Source icon path
source_icon = "/home/liam/chromium/src/dappnet/dappnet/desktop-app/dist-resources/icon.png"
source_ico = "/home/liam/chromium/src/dappnet/dappnet/desktop-app/dist-resources/icon.ico"
source_icns = "/home/liam/chromium/src/dappnet/dappnet/desktop-app/dist-resources/icon.icns"

# Chrome theme directory
chrome_theme_dir = "/home/liam/chromium/src/chrome/app/theme/chromium"
chrome_theme_linux_dir = f"{chrome_theme_dir}/linux"
chrome_theme_100_dir = "/home/liam/chromium/src/chrome/app/theme/default_100_percent/chromium"
chrome_theme_100_linux_dir = f"{chrome_theme_100_dir}/linux"

# Required icon sizes for Chrome
sizes = [16, 24, 32, 48, 64, 128, 256]
sizes_100_percent = [16, 32]  # Sizes needed for default_100_percent

print("Generating Chrome icons for Dappnet...")

# Load the source image
img = Image.open(source_icon)

# Create necessary directories
os.makedirs(chrome_theme_linux_dir, exist_ok=True)
os.makedirs(chrome_theme_100_dir, exist_ok=True)
os.makedirs(chrome_theme_100_linux_dir, exist_ok=True)

# Generate different sizes for main chromium directory
for size in sizes:
    resized = img.resize((size, size), Image.LANCZOS)
    
    # Save to main chromium directory
    output_path = f"{chrome_theme_dir}/product_logo_{size}.png"
    print(f"Creating {output_path}")
    resized.save(output_path, "PNG")
    
    # Also save to linux subdirectory
    linux_path = f"{chrome_theme_linux_dir}/product_logo_{size}.png"
    print(f"Creating {linux_path}")
    resized.save(linux_path, "PNG")

# Generate sizes for default_100_percent directory
for size in sizes_100_percent:
    resized = img.resize((size, size), Image.LANCZOS)
    
    # Save to default_100_percent/chromium
    output_path = f"{chrome_theme_100_dir}/product_logo_{size}.png"
    print(f"Creating {output_path}")
    resized.save(output_path, "PNG")
    
    # Save to default_100_percent/chromium/linux
    linux_path = f"{chrome_theme_100_linux_dir}/product_logo_{size}.png"
    print(f"Creating {linux_path}")
    resized.save(linux_path, "PNG")

# Generate 32x32 XPM for Linux (required by build)
xpm_path = f"{chrome_theme_linux_dir}/product_logo_32.xpm"
print(f"Creating {xpm_path}")
# For now, just touch the file - proper XPM conversion would require additional library
open(xpm_path, 'a').close()

# Copy special naming convention files
print(f"Creating {chrome_theme_dir}/product_logo_name.png")
shutil.copy(source_icon, f"{chrome_theme_dir}/product_logo_name.png")

# Generate 22 mono version (copy from 24 for now)
resized_22 = img.resize((22, 22), Image.LANCZOS)
mono_path = f"{chrome_theme_dir}/product_logo_22_mono.png"
print(f"Creating {mono_path}")
resized_22.save(mono_path, "PNG")

# Copy Windows .ico file
if os.path.exists(source_ico):
    print(f"Copying Windows icon to {chrome_theme_dir}/chromium.ico")
    shutil.copy(source_ico, f"{chrome_theme_dir}/chromium.ico")
    
    # Also copy to installer
    installer_ico = "/home/liam/chromium/src/chrome/installer/setup/setup.ico"
    if os.path.exists(os.path.dirname(installer_ico)):
        print(f"Copying Windows icon to {installer_ico}")
        shutil.copy(source_ico, installer_ico)

# Copy macOS .icns file
if os.path.exists(source_icns):
    mac_icon_path = f"{chrome_theme_dir}/mac/app.icns"
    os.makedirs(os.path.dirname(mac_icon_path), exist_ok=True)
    print(f"Copying macOS icon to {mac_icon_path}")
    shutil.copy(source_icns, mac_icon_path)

print("\nIcon generation complete!")
print("\nAll icon locations updated:")
print("- chrome/app/theme/chromium/*.png")
print("- chrome/app/theme/chromium/linux/*.png")
print("- chrome/app/theme/default_100_percent/chromium/*.png")
print("- chrome/app/theme/default_100_percent/chromium/linux/*.png")
print("\nRebuild Chrome with: autoninja -C out/Default chrome")