# UI DevTools Overview

UI DevTools allows UI developers to inspect the Chrome desktop UI system like a webpage using the frontend DevTools Inspector. It is
currently supported on Linux, Windows, Mac, and ChromeOS.

* [Old Ash Doc](https://www.chromium.org/developers/how-tos/inspecting-ash)
* [Backend Source Code](https://cs.chromium.org/chromium/src/components/ui_devtools/)
* [Inspector Frontend Source Code](https://chromium.googlesource.com/devtools/devtools-frontend)

## How to run

1. Run Chromium with default port 9223 and start Chromium with UI DevTools flag:

        $ out/Default/chrome --enable-ui-devtools

    * If you want to use different port, add port in the flag `--enable-ui-devtools=<port>`.
2. In the Chrome Omnibox, go to chrome://inspect#other and click `inspect` under UIDevToolsClient.
    * Direct link is chrome-devtools://devtools/bundled/inspector.html?ws=localhost:9223/0.


## How to Use

### Elements Tree

View, Window, and Widget elements are displayed in the hierarchal elements tree. To expand the elements
tree, right click the root element and then select Expand Recursively from the context menu.
Then, to inspect an element's properties, click on its elements node.

![expand elements]

### Views Property Inspection

When an element in the elements tree is selected, the property panel on the right side displays the
element's properties. All elements have the basic properties (height, width, x, y, visibility).
For Window and View elements, if that element has Layer properties, they will be displayed in
a separate section.

![browser frame properties]

For View elements with Metadata properties, each parent class's properties from the Metadata
will be displayed in an additional section.

![image view properties]

Elements' basic properties can be modified from the properties panel and changes will be shown.

![edit property]

### Inspect Mode

To enter inspect mode, click the inspect icon at top left corner of UI Devtools.

![inspector button]

Hovering over a UI element highlights that element with a blue rectangular border and reveals
the element's node in the elements tree.

![hovering over elements]

Clicking on a highlighted element will pin that element. Clicking the corresponding node
in the elements tree will reveal that element's properties.

![hover and inspect]

When an element is pinned, hovering over other elements will reveal the vertical and horizontal
distance between the two elements.

![hovering distances]

To exit inspect mode, either click the inspect icon again or press the Esc key.

### Sources Panel

In the properties panel, each section has a source link in the upper right corner. When right-clicked,
the context menu shows the option to "Open in new tab," which opens the class's header file in Chromium
code search.

![properties panel right click link]

If the link is simply clicked, it will open the source code in the sources panel of UI DevTools, which
is read in from local files.

![sources panel]

### View Bounds Highlighting

Red border rectangles around each View element can be drawn using the command Ctrl+R (Meta+R for mac).
The rectangles can be toggled off and on using the same command.

![debug bounds rectangles]

### Bubble Locking

In order to inspect a bubble, the command Ctrl+Shift+R (Meta+Shift+R for mac) locks bubbles to prevent
them from dismissing upon losing focus. This allows a bubble's inner elements to be inspected. Bubble
locking can be toggled off and on using the same command.

![lock and inspect bubble]

### UI Element Tree Search

In the elements panel, Ctrl+F to open the search bar at the bottom. The search functionality allows
developers to search the UI element tree quickly by name, tag <>, and style properties. 
The search can do substring matches or exact matches (specified with quotations). The search returns
all matches and highlights the specific nodes on the tree that are matched. The up and down arrows
on the right of the search bar or ENTER are used to traverse through the matches.

![ui element tree search]

The search feature can also search the element's style properties in the UI element tree. The special
key word 'style:' must be typed in the search bar.

![search style]

[expand elements]: images/expand_elements.gif
[browser frame properties]: images/browser_frame_properties.png
[image view properties]: images/image_view_properties.png
[edit property]: images/edit_property.gif
[inspector button]: images/inspector_button.png
[hovering over elements]: images/hovering_over_elements.gif
[hover and inspect]: images/hover_and_inspect.gif
[hovering distances]: images/hovering_distances.gif
[properties panel right click link]: images/properties_panel_right_click_link.png
[sources panel]: images/sources_panel.png
[debug bounds rectangles]: images/debug_bounds_rectangles.png
[lock and inspect bubble]: images/lock_and_inspect_bubble.gif
[ui element tree search]: images/ui_element_tree_search.gif
[search style]: images/search_style.png
