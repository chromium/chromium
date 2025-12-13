# Find-in-Page (`finder`)

This directory contains the core logic for Blink's find-in-page feature. It is
responsible for searching a document represented in a DOM tree for a given
query string and reporting the results.

## Overview

The find-in-page mechanism is initiated by a user action (e.g., Ctrl+F). The
`TextFinder` class is the main entry point, which coordinates the search
process. The search can be performed synchronously or asynchronously to avoid
blocking the main thread on large documents.

## Key Components

*   **`TextFinder`**: The primary class that orchestrates the find-in-page
    operation. It takes a search query and `FindOptions` and uses a
    `Editor::FindRangeOfString()` to perform the search.

*   **`FindOptions`**: A simple struct that holds the options for the search,
    such as `forward`, `match_case`, `find_next`, etc.

*   **`FindBufferRunner`**: An abstraction of a text find operation. It
    provides methods to find matches for a given query. It is used to find text
    fragment specified in a URL, not for the find-in-page UI. There are two main
    implementations:
    *   **`SyncFindBuffer`**: Performs a synchronous search on the document's
        text.
    *   **`AsyncFindBuffer`**: Performs an asynchronous search, breaking the
        document into chunks to avoid blocking the main thread.

*   **`FindTaskController`**: Manages the execution of asynchronous find tasks,
    ensuring that they are cancelable and do not overwhelm the system.

*   **`FindBuffer`**: Finds the specified query within a specified range of a
    document represented as a DOM.

*   **`FindResults`**: A data structure that stores the results of a find
    operation, including the number of matches and the active match index.

*   **`FindInPageCoordinates`**: A helper class to compute the screen
    coordinates of the found text ranges. This is used for highlighting the
    results and scrolling them into view.
