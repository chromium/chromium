This is the SwiftUI Previews-only Xcode project. See ../shared/README.md
for instructions on how to add more code shared with Chromium.

# Deps
Since this project is not generated with GN, any external
(to this folder) dependencies are added directly as frameworks.
Frameworks can be created and will be caught by the build system if named
properly, for instance `import ios_chrome_common_ui_colors_swift` will
import the code of the target `ios_chrome_common_ui_colors_swift`. Then
add the source set files to the associated framework/target.
