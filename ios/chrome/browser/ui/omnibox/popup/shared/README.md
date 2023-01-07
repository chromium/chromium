This code is shared between Chromium iOS and the SwiftUI Previews-only Xcode
project file in `../swiftui_previews`. 

If you need to add more shared code, add it here first, then open the
`.xcodeproj` and drag-and-drop it there, and make sure not to check "Copy".
Double-check that the file didn't copy or move. 

Then add this file to `../BUILD.gn` as usual. 
