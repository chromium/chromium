# Running Old Devtools

This package launches your current version of Chromium, and an old version of Chromium.
It then opens the DevTools of the old Chromium inside the new Chromium. This can be
used to test devtools_compatibility.js. Remember to recompile after making changes
to devtools_compatibility.js.

## Usage
First run `npm install` in this directory. Then run `node index.js <revision_number>`.
