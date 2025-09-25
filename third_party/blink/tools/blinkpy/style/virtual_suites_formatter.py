# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
from typing import Any, Dict, List, Union


def format_json_with_comments(
        json_content: List[Union[str, Dict[str, Any]]]) -> str:
    """Formats and sorts a list of objects and string comments from parsed JSON.

    The sorting is based on the 'prefix' key of the objects. String comments
    are kept with the object that immediately follows them. Header comments
    at the top of the file are also preserved. Empty lines are represented as
    empty string entries "". All comments before a `__BEGIN_SUITES__` entry
    are considered header comments and are not sorted.

    Args:
        json_content: The list of objects and strings to format and sort.

    Returns:
        The formatted and sorted content as a JSON string.
    """
    # --- Step 1: Separate header comments from the main content ---
    # Header comments are comments before the `__BEGIN_SUITES__` marker.
    try:
        header_boundary = json_content.index('__BEGIN_SUITES__') + 1
    except ValueError:
        header_boundary = 0
    header_comments = json_content[:header_boundary]
    processing_data = json_content[header_boundary:]

    # --- Step 3: Group comments with their corresponding objects ---
    sortable_items = []
    current_comments = []
    for item in processing_data:
        if isinstance(item, str):
            current_comments.append(item)
        elif isinstance(item, dict):
            sortable_items.append((current_comments, item))
            current_comments = []
    # Any remaining comments at the end of the file.
    trailing_comments = current_comments

    # --- Step 4: Sort the groups based on the object's 'prefix' key ---
    sortable_items.sort(key=lambda group: group[1]['prefix'])

    # --- Step 5: Reconstruct the list in the new sorted order ---
    final_sorted_list = list(header_comments)
    for comments, obj in sortable_items:
        final_sorted_list.extend(comments)
        final_sorted_list.append(obj)
    final_sorted_list.extend(trailing_comments)

    # --- Step 6: Return the sorted data as a string ---
    return json.dumps(final_sorted_list, indent=2) + '\n'
