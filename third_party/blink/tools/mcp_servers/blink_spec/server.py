#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""MCP server for dealing with spec and github for Blink work"""

import json
import os
import re
import requests

# vpython-provided modules
# pylint: disable=import-error
from mcp.server import fastmcp
# pylint: enable=import-error

GITHUB_API_KEY = os.environ.get("BLINK_SPEC_GITHUB_API_KEY", "")

mcp = fastmcp.FastMCP(name="Blink Spec")


# TODO: Need to figure out how to generalize this to all well known specs.
@mcp.tool()
def get_github_issues_repo_for_spec(spec: str) -> str:
    """
    Given a specification abbreviation (e.g. CSS, or CSSWG), returns the github
    issues link where the issues for that spec are located (e.g.
    https://github.com/w3c/csswg-drafts/issues).

    Currently supported specs:
     - CSS (CSSWG)
     - HTML (WHATWG)
    """

    spec = spec.upper()
    if spec == "CSS" or spec == "CSSWG":
        return f"https://github.com/w3c/csswg-drafts/issues"
    elif spec == "HTML" or spec == "WHATWG":
        return f"https://github.com/whatwg/html/issues"

    return "Unknown spec: {spec}"


@mcp.tool()
def get_github_issue_with_comments(issue_url: str) -> str:
    """
    Fetches the original post and all comments from a GitHub issue,
    returning them as a single JSON string.

    Args:
        issue_url (str): The full URL of the GitHub issue.
                         Example: 'https://github.com/jlowin/fastmcp/issues/1'

    Returns:
        A JSON formatted string containing the original post and all comments,
        each with an author, date, and comment. Returns an error message on failure.
    """

    pattern = r"https://github\.com/([^/]+)/([^/]+)/issues/(\d+)"
    match = re.match(pattern, issue_url)

    if not match:
        error_msg = "Error: Invalid GitHub issue URL format."
        return error_msg

    owner, repo, issue_number = match.groups()

    issue_api_url = f"https://api.github.com/repos/{owner}/{repo}/issues/{issue_number}"
    headers = {
        "Authorization": f"Bearer {GITHUB_API_KEY}",
        "Accept": "application/vnd.github.v3+json",
        "X-GitHub-Api-Version": "2022-11-28"
    }

    all_posts = []

    try:
        # Get the original post.
        issue_response = requests.get(issue_api_url,
                                      headers=headers,
                                      timeout=10)
        issue_response.raise_for_status()
        issue_data = issue_response.json()

        all_posts.append({
            "author": issue_data['user']['login'],
            "date": issue_data['created_at'],
            "comment": issue_data['body'] or ""
        })

        # Get comments.
        comments_url = issue_api_url + "/comments"
        while comments_url:
            comments_response = requests.get(comments_url,
                                             headers=headers,
                                             timeout=10)
            comments_response.raise_for_status()
            comments_data = comments_response.json()

            for comment in comments_data:
                all_posts.append({
                    "author": comment['user']['login'],
                    "date": comment['created_at'],
                    "comment": comment['body'] or ""
                })

            # Handle pagination.
            comments_url = None
            link_header = comments_response.headers.get('link')
            if not link_header:
                break

            # The format is <url>; rel="type", ...
            link_pattern = re.compile(r'<([^>]+)>;\s*rel="([^"]+)"\s*(?=,|$)')
            for match in link_pattern.finditer(link_header):
                url, rel = match.groups()
                if rel != 'next':
                    continue
                comments_url = url
                break
    except requests.exceptions.HTTPError as e:
        response_json = e.response.json()
        api_message = response_json.get('message',
                                        'No message field in API response.')
        error_msg = f"Error: API request failed with status {e.response.status_code}. API Message: {api_message}"
        return error_msg
    except requests.exceptions.RequestException as e:
        error_msg = f"Error: A network request exception occurred. {e}"

        return error_msg

    return json.dumps(all_posts, indent=2)


if __name__ == '__main__':
    if GITHUB_API_KEY == "":
        print(
            "Error: BLINK_SPEC_GITHUB_API_KEY not set. See //agents/extensions/blink_spec/README.md"
        )
    else:
        print("Starting Blink Spec MCP server")
        mcp.run()
