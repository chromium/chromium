# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining consoles in a distributed fashion.

The `console_view`, `console_view_entry`, `ordering` and
`overview_console_view` functions defined in this module enable defining
console views in a distributed manner with the order of the entries
being customizable and ignoring declaration order of the builders. They
can also be accessed through `consoles.console_view`,
`consoles.console_view_entry`, `consoles.ordering` and
`consoles.overview_console_view`, respectively.

The `list_view` function defined in this module enables defining a list
view in a distributed manner with the order of the entries being
deterministic and ignoring declaration order of the builders. It can
also be accessed through `consoles.list_view`.

The `defaults` struct provides module-level defaults for the arguments
to `console_view` and `overview_console_view`. Calling `defaults.set`
with keyword argument corresponding to the parameters that support
module-level defaults will set the values use when no explicit values
are provided in a declaration. Can also be accessed through
`consoles.defaults`.
"""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./args.star", "args")
load("./branches.star", "branches")
load("./nodes.star", "nodes")

defaults = args.defaults(
    header = None,
    repo = None,
    refs = None,
)

_GEN_JSON_ROOT_DIR = "console_views"

_CONSOLE_VIEW_ORDERING = nodes.create_unscoped_node_type("console_view_ordering")
_OVERVIEW_CONSOLE_ORDERING = nodes.create_unscoped_node_type("overview_console_ordering")
_CONSOLE_VIEW = nodes.create_unscoped_node_type("console_view")
_BUILDER = nodes.create_unscoped_node_type("builder")

def _console_view_ordering_impl(_ctx, *, console_name, ordering):
    key = _CONSOLE_VIEW_ORDERING.add(console_name, props = {
        "ordering": ordering,
    })
    graph.add_edge(keys.project(), key)
    return graph.keyset(key)

_console_view_ordering = lucicfg.rule(impl = _console_view_ordering_impl)

def _overview_console_view_ordering_impl(_ctx, *, console_name, top_level_ordering):
    key = _OVERVIEW_CONSOLE_ORDERING.add(console_name, props = {
        "top_level_ordering": top_level_ordering,
    })
    graph.add_edge(keys.project(), key)
    return graph.keyset(key)

_overview_console_view_ordering = lucicfg.rule(impl = _overview_console_view_ordering_impl)

def _category_join(parent, category):
    return "|".join([c for c in (parent, category) if c])

def _level_sort_key(category, ordering):
    """Compute the key for a single level of ordering.

    A key that can be used to sort categories/short names at the same
    category nesting level.
    """
    for i, c in enumerate(ordering):
        if c == category:
            # We found the category in the ordering, so the index in the ordering is
            # sufficient for sorting
            return (i,)

    # We didn't find the category, the key uses:
    # 1. The length of the ordering so that it sorts after all the categories in
    #    the ordering
    # 2. The category itself, which lexicographically sorts all of the categories
    #    that do not match the ordering
    return (len(ordering), category)

def _builder_sort_key(console_ordering, category, short_name, name):
    """Compute the key for a builder.

    Builders are sorted lexicographically by the sequence of category
    components, then lexicographically by the short name, then by the
    builder names. The ordering for the console_view modifies the sorting
    for category components and short names for given prefixes of the
    category component sequence. Builders with no short name will sort
    before builders with a short name for a given category, which cannot
    be modified by the ordering.

    Returns:
      A key that can be used to sort builder entries within the same console.
    """
    current_category = None

    # Build the category key as a sequence of the keys for each level
    category_key = []
    if category:
        for c in category.split("|"):
            ordering = console_ordering.get(current_category, [])
            if type(ordering) == type(""):
                ordering = console_ordering[ordering]
            if type(ordering) == type(struct()):
                ordering = ordering.categories
            category_key.append(_level_sort_key(c, ordering))
            current_category = _category_join(current_category, c)

    short_name_key = ()
    if short_name:
        ordering = console_ordering.get(category, [])
        if type(ordering) == type(""):
            ordering = console_ordering[ordering]
        short_name_ordering = getattr(ordering, "short_names", [])
        short_name_key = _level_sort_key(short_name, short_name_ordering)

    return (
        category_key,
        short_name_key,
        name,
    )

def _get_console_ordering(console_name):
    """Get the ordering dict used for sorting entries of a console_view.

    Returns:
      The ordering dict used to sort entries of the console_view with the
      given name or None if the name does not refer to a console_view with
      an ordering.
    """
    node = _CONSOLE_VIEW_ORDERING.get(console_name)
    return node.props.ordering if node != None else None

def _get_console_view_key_fn(console_name):
    """Get the key function for sorting entries of a console_view.

    Returns:
      The key function used to sort entries of the console_view with the
      given name or None if the name does not refer to a console_view with
      an ordering.
    """
    ordering = _get_console_ordering(console_name)
    if ordering == None:
        return None

    def key_fn(b):
        return _builder_sort_key(ordering, b.category, b.short_name, b.name)

    return key_fn

def _get_overview_console_view_key_fn(console_name):
    """Get the key function for sorting overview_console_view entries.

    Returns:
      The key function used to sort entries of the overview_console_view
      with the given name or None if the name does not refer to an
      overview_console_view.
    """
    overview_console_ordering = _OVERVIEW_CONSOLE_ORDERING.get(console_name)
    if overview_console_ordering == None:
        return None

    top_level_ordering = overview_console_ordering.props.top_level_ordering

    def key_fn(b):
        if not b.category:
            fail("Builder {} must have a category".format(b))
        category_components = b.category.split("|", 1)

        subconsole = category_components[0]
        subconsole_sort_key = _level_sort_key(subconsole, top_level_ordering)

        builder_sort_key = ()
        subconsole_ordering = _get_console_ordering(subconsole)
        if subconsole_ordering != None:
            category = ""
            if len(category_components) > 1:
                category = category_components[1]
            builder_sort_key = _builder_sort_key(
                subconsole_ordering,
                category,
                b.short_name,
                b.name,
            )

        return (
            subconsole_sort_key,
            builder_sort_key,
        )

    return key_fn

def ordering(*, short_names = None, categories = None):
    """Specifies the sorting behavior for a category.

    Args:
      short_names - A list of strings that specifies the order short names
        should appear for builders in the same category. Builders without
        short names will appear before all others and builders with short
        names that do not appear in the list will be sorted
        lexicographically after short names that do appear in the list. By
        default, short names are sorted lexicographically.
      categories - A list of strings that specifies the order the next
        category component should appear for builders with matching
        category prefix. Builders without any additional category
        components will appear before all others and builders whose next
        category component do not appear in the list will be sorted
        lexicographically by the next category component. By default, the
        next category components are sorted lexicographically.
    """
    return struct(
        short_names = short_names or [],
        categories = categories or [],
    )

def console_view(
        *,
        name,
        branch_selector = branches.selector.MAIN,
        ordering = None,
        record_json = False,
        **kwargs):
    """Create a console view, optionally providing an entry ordering.

    Args:
      name - The name of the console view.
      branch_selector - A branch selector value controlling whether the
        console view definition is executed. See branches.star for
        more information.
      ordering - A dictionary defining the ordering of categories for the
        console. If not provided, the console will not be sorted.

        The keys of the dictionary indicate the category that the values
        applies the sorting to and can take one of two forms:
        1.  None: Controls the ordering of the top-level categories and/or
            the short names of builders that have no category.
        2.  str: Category string to apply the ordering to the next nested
            level of categories and/or the short names of builders with
            that category. Arbitrary strings can be used also, which can
            be used as aliases for other entries to refer to.

        The value for each entry defines the ordering to be applied to
        builders that have matched the sequence of category components
        identified by the key and can take one of two forms:
        1.  struct created using `consoles.ordering`: See
            `consoles.ordering` for details.
        2.  list of category components: Equivalent to a
            `consoles.ordering` call that only specifies `categories`
            with the given list.
        3.  str: An alias for another category. The string must be another
            key in the dict. The ordering will be looked up by that key
            instead.
      record_json - A boolean indicating whether to generate a json file that
        lists all builders included in this console view.
      kwargs - Additional keyword arguments to forward on to
        `luci.console_view`. The header, repo and refs arguments support
        module-level defaults.
    """
    if not branches.matches(branch_selector):
        return

    kwargs["header"] = defaults.get_value_from_kwargs("header", kwargs)
    kwargs["repo"] = defaults.get_value_from_kwargs("repo", kwargs)
    kwargs["refs"] = defaults.get_value_from_kwargs("refs", kwargs)
    luci.console_view(
        name = name,
        **kwargs
    )

    _console_view_ordering(
        console_name = name,
        ordering = ordering or {},
    )

    graph.add_edge(keys.project(), _CONSOLE_VIEW.add(
        name,
        props = {
            "record_json": record_json,
        },
    ))

def overview_console_view(*, name, top_level_ordering, **kwargs):
    """Create an overview console view.

    An overview console view is a console view that contains a subset of
    entries from other consoles. The entries from each console will have
    that console's name prepended to the entries' categories and will
    appear in the same order as they do in that console. The ordering of
    entries from different consoles is controlled by the
    `top_level_ordering` parameter.

    Args:
      name - The name of the console view.
      top_level_ordering - A list of strings defining the order that
        entries from different consoles will appear. Entries will be
        sorted by the name of the console they come from, appearing in the
        same order as in top_level_ordering. Entries from consoles whose
        name does not appear in the list will be sorted lexicographically
        by the console name and appear after entries whose console does
        appear in the list.
      kwargs - Additional keyword arguments to forward on to
        `luci.console_view`. The header and repo arguments support
         module-level defaults.
    """
    kwargs["header"] = defaults.get_value_from_kwargs("header", kwargs)
    kwargs["repo"] = defaults.get_value_from_kwargs("repo", kwargs)
    luci.console_view(
        name = name,
        **kwargs
    )

    _overview_console_view_ordering(
        console_name = name,
        top_level_ordering = top_level_ordering,
    )

def console_view_entry(
        *,
        branch_selector = branches.selector.ALL_BRANCHES,
        console_view = None,
        category = None,
        short_name = None):
    """Specifies the details of a console view entry.

    See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md#luci.console_view_entry
    for more details on the arguments.

    Args:
      branch_selector - A branch selector value controlling whether
        console view entry definition is executed. The console view
        entry is only defined if the associated builder is defined based
        on its branch selector value. By default, the console view entry
        will be defined whenever the associated builder is defined. See
        branches.star for more information.
      console_view - The console view to add an entry for the associated
        builder to. By default, the entry will be for a console with the
        same name as the builder's builder group, which the builder must
        have. At most one `console_view_entry` for a builder can omit
        the `console_view` value.
      category - The category of the builder in the console.
      short_name - The short name of the builder in the console.

    Returns:
      A struct that can be passed to the `console_view_entry` argument
      of `builders.builder` in order to create a console view entry for
      the builder.
    """
    return struct(
        branch_selector = branch_selector,
        console_view = console_view,
        category = category,
        short_name = short_name,
    )

def register_builder_to_console_view(
        console_view,
        category,
        short_name,
        project,
        bucket,
        group,
        builder):
    """Create a node of "builder" kind and link it to the console view.

    Args:
        console_view: (string) The console view name.
        category - (string) The category of the builder in the console.
        short_name - (string) The short name of the builder in the console.
        project: (string) The project name.
        bucket: (string) The bucket name.
        group: (string) The builder group name.
        builder: (string) The builder name.
    """
    graph.add_edge(
        _CONSOLE_VIEW.key(console_view),
        _BUILDER.add(
            "{}/{}/{}".format(console_view, bucket, builder),
            props = {
                "console_view": console_view,
                "category": category,
                "short_name": short_name,
                "project": project,
                "bucket": bucket,
                "group": group,
                "builder": builder,
            },
        ),
    )

def _sorted_list_view_graph_key(console_name):
    return graph.key("@chromium", "", "sorted_list_view", console_name)

def _get_list_view_key_fn(console_name):
    sorted_list_view = graph.node(_sorted_list_view_graph_key(console_name))
    if sorted_list_view == None:
        return None
    return lambda b: b.name

def _sorted_list_view_impl(_ctx, *, console_name):
    key = _sorted_list_view_graph_key(console_name)
    graph.add_node(key)
    graph.add_edge(keys.project(), key)
    return graph.keyset(key)

_sorted_list_view = lucicfg.rule(impl = _sorted_list_view_impl)

def list_view(*, name, branch_selector = branches.selector.MAIN, **kwargs):
    """Create a sorted list view.

    The entries in the list view will be sorted by the builder's name.

    Args:
      name - The name of the list view.
      branch_selector - A branch selector value controlling whether the
        console view definition is executed. See branches.star for
        more information.
      kwargs - Additional keyword arguments to forward on to
        `luci.list_view`.
    """
    if not branches.matches(branch_selector):
        return

    luci.list_view(
        name = name,
        **kwargs
    )

    _sorted_list_view(
        console_name = name,
    )

def _sort_consoles(ctx):
    if "luci/luci-milo.cfg" not in ctx.output:
        return
    milo = ctx.output["luci/luci-milo.cfg"]
    consoles = []
    for console in milo.consoles:
        if not console.builders:
            continue
        key_fn = (_get_console_view_key_fn(console.id) or
                  _get_overview_console_view_key_fn(console.id) or
                  _get_list_view_key_fn(console.id))
        if key_fn:
            console.builders = sorted(console.builders, key_fn)
        consoles.append(console)

def _console_view_json(ctx):
    """Generator callback for generating "{console_view_name}.json" files.

    Each console with "record_json = True" will have a json file inside
    the _GEN_JSON_ROOT_DIR directory. The format of the jason is:
    'project_name/bucket_name': {
      'builder_group': ['builder1', ..]
    }

    Args:
        ctx: the context object.
    """
    for console_node in graph.children(keys.project(), _CONSOLE_VIEW.kind):
        if not console_node.props.record_json:
            continue

        console_dict = {}
        for builder_node in graph.children(console_node.key):
            props = builder_node.props
            console_dict.setdefault(
                "{}/{}".format(props.project, props.bucket),
                {},
            ).setdefault(
                props.group,
                {},
            )[props.builder] = {
                "category": props.category,
                "short_name": props.short_name,
            }
        if console_dict:
            ctx.output["{}/{}.json".format(_GEN_JSON_ROOT_DIR, console_node.key.id)] = json.indent(
                json.encode(console_dict),
                indent = "  ",
            )

lucicfg.generator(_sort_consoles)
lucicfg.generator(_console_view_json)

consoles = struct(
    # Module-level defaults for consoles functions
    defaults = defaults,

    # Functions for declaring automatically maintained console views
    console_view = console_view,
    console_view_entry = console_view_entry,
    ordering = ordering,
    overview_console_view = overview_console_view,

    # Functions for declaring automatically maintained list views
    list_view = list_view,
)
