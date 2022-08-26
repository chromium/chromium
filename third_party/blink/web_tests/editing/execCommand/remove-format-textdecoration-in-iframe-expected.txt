This testcase is to test crash scenario when designMode is set on document and RemoveFormat is called. Expected result is that crash should not happen and underline should be removed from all the selected text
| "<#selection-anchor>This Test should not crash.
        "
| <iframe>
|   onload="selectAndRemoveFormat()"
| "
        "
| <p>
|   "PASS"
| "
    "

FRAME 0:
| <head>
| <body>
