# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library for defining console headers.

A header can be defined using the `header` function, with `console_group`,
`link_group` and `link` being used to define the components of the header. The
returned object can be specified as the header when defining a console view.

All functions can also be accessed via the `headers` struct.
"""

load("//lib/branches.star", "branches")

def _remove_none(l):
    return [e for e in l if e != None]

def _remove_none_values(d):
    return {k: v for k, v in d.items() if v != None}

def oncall(
        *,
        name,
        url,
        show_primary_secondary_labels = None,
        branch_selector = branches.selector.MAIN):
    """Define an oncall rotation to appear in a console header.

    Args:
      name - The name to display the oncall as.
      url - The URL to read the oncall rotation from.
      show_primary_secondary_labels - Whether to show labels indicating the
        primary and secondary next to the current primary and secondary for the
        oncall.
      branch_selector - A branch selector value controlling whether the
        oncall definition is executed. See branches.star for more information.
    """
    if not branches.matches(branch_selector):
        return None
    return _remove_none_values(dict(
        name = name,
        url = url,
        show_primary_secondary_labels = show_primary_secondary_labels,
    ))

def link_group(*, name, links, branch_selector = branches.selector.MAIN):
    """Define a link group to appear in a console header.

    A link group is a set of links that are displayed together under a common
    heading.

    Args:
      name - The name of the link group, used as the heading for the group.
      links - A list of objects returned from `link` defining the links
        belonging to the group.
      branch_selector - A branch selector value controlling whether the
        link group definition is executed. See branches.star for more
        information.
    """
    if not branches.matches(branch_selector):
        return None
    links = _remove_none(links)
    if not links:
        return None
    return _remove_none_values(dict(
        name = name,
        links = links,
    ))

def link(*, url, text, alt = None, branch_selector = branches.selector.MAIN):
    """Define a link to appear in a console header.

    Args:
      url - The URL to link to.
      text - The display text for the link.
      alt - The alt text for the link. This is supposed to be the hover text
        according to the proto
        https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/milo/api/config/project.proto,
        but doesn't appear to produce any hover text.
      branch_selector - A branch selector value controlling whether the
        link definition is executed. See branches.star for more information.
    """
    if not branches.matches(branch_selector):
        return None
    return _remove_none_values(dict(
        url = url,
        text = text,
        alt = alt,
    ))

def console_group(
        *,
        console_ids,
        title = None,
        branch_selector = branches.selector.MAIN):
    """Define a console group.

    A console group is a set of consoles that will be displayed in the header.
    For each console in the group, the name of the console will appear along
    with a square for each builder in that console, with each square colored
    according to the status of the last completed build for the builder.

    Args:
      console_ids - A list of IDs of the consoles to display in the group.
      title - An optional title to apply to the console group, returned from
        `link`. If provided, the console group will have a box drawn around it
        with the title appearing at the top of the box.
    """
    if not branches.matches(branch_selector):
        return None
    console_ids = _remove_none(console_ids)
    if not console_ids:
        return None
    return _remove_none_values(dict(
        title = title,
        console_ids = console_ids,
    ))

def header(
        *,
        oncalls = None,
        link_groups = None,
        console_groups = None,
        tree_status_host = None,
        tree_name = None):
    """Define a console header.

    The returned object can be specified as the header when defining a console
    view.

    Args:
      oncalls - Optional list of oncalls returned from `oncall` to to display in
        the header.
      links - Optional list of link groups returned from `link_group` to display
        in the header.
      console_groups - Optional list of console groups returned from
        `console_group` to display in the header.
      tree_status_host - DEPRECATED: Use tree_name instead.
        Optional URL of the tree status host.
        If provided, the current tree status is displayed at the top of the header,
        colored according to the status of the tree.
      tree_name - Name of the tree in LUCI Tree Status app.
        If provided, the current tree status is displayed at the top of the header,
        colored according to the status of the tree.
    """
    return _remove_none_values(dict(
        oncalls = _remove_none(oncalls or []),
        links = _remove_none(link_groups or []),
        console_groups = _remove_none(console_groups or []),
        tree_status_host = tree_status_host,
        tree_name = tree_name,
    ))

headers = struct(
    console_group = console_group,
    header = header,
    link = link,
    link_group = link_group,
    oncall = oncall,
)
