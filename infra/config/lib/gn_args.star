# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for specifying GN args for builders.

More details can be found in design doc: go/gn-args-in-starlark-dd
"""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./chrome_settings.star", "per_builder_outputs_config")
load("./nodes.star", "nodes")

_GN_CONFIG = nodes.create_unscoped_node_type("gn_config")

_GN_ARGS_FILE_NAME = "gn-args.json"

def _get_gn_args_resolver():
    resolved_gn_args_by_config_node = {}

    def merge_gn_args_into_dict(dst, args_file = "", gn_args = {}):
        """Merge imported GN file and GN args into the specified dict.

        The function modifies the dst dict and has no return values.
        The entries from the input parameters will take priority and overwrite
        those duplicated entries in the dst dict as the result of the merge.

        Args:
            dst: (dict) The dict into which the rest of the inputs will be
                merged.
            args_file: (string) The string path of an imported GN file.
            gn_args: (dict) A dict of GN arg key-value pairs.
        """
        if args_file and dst["args_file"]:
            fail("Each GN config can only contain a single args_file")
        dst["args_file"] = args_file

        dst["gn_args"].update(gn_args)

    def visitor(_, children):
        return [c for c in children if c.key.kind == _GN_CONFIG.kind]

    def resolve(gn_config_node):
        """Resolve the GN args of a given GN config node.

        Essentially, this function traverses the graph by DFS with memoization.

        Args:
            gn_config_node: (node of "gn_config" kind) The node to resolve
                GN args for; typically a node representing a builder.

        Returns:
            A dict representing the GN args of the input node, with entries:
                * args_file: a string path of an imported GN file.
                * gn_args: a dict of GN arg key-value pairs.
        """
        for n in graph.descendants(
            gn_config_node.key,
            visitor = visitor,
            order_by = graph.DEFINITION_ORDER,
            topology = graph.DEPTH_FIRST,
        ):
            if n in resolved_gn_args_by_config_node:
                continue

            gn_args = {
                "args_file": "",
                "gn_args": {},
            }

            # Merge GN args of child nodes in DEFINITION_ORDER of the edges.
            for child in graph.children(
                n.key,
                kind = _GN_CONFIG.kind,
                order_by = graph.DEFINITION_ORDER,
            ):
                child_config = resolved_gn_args_by_config_node[child]
                merge_gn_args_into_dict(gn_args, child_config["args_file"], child_config["gn_args"])

            # Merge GN args of the current node.
            merge_gn_args_into_dict(
                gn_args,
                n.props.args_file,
                n.props.gn_args,
            )

            # Memoize.
            resolved_gn_args_by_config_node[n] = gn_args

        return dict(resolved_gn_args_by_config_node[gn_config_node])

    return resolve

def _create_gn_config_node(name, gn_args = {}, configs = [], args_file = "", builder_group = ""):
    """Create a node of "gn_config" kind and place it into the graph.

    Args:
        name: (string) The name of the GN config.
        gn_args: (dict) A dict of GN arg key-value pairs.
        configs: ([string]) A list of GN config name strings.
        args_file: (string) The string path of an imported GN file.
        builder_group: (string) The builder group of a builder node.

    Returns:
        The node key of the created node.
    """
    gn_config_key = _GN_CONFIG.add(name, props = {
        "gn_args": gn_args,
        "args_file": args_file,
        "builder_group": builder_group,
    })

    # Create edges in the graph from the new node to its included nodes.
    # Edge creation follows the order of the configs in the list. This order
    # also determines the final value of a GN arg when there are duplicated
    # entries of GN args for a builder, i.e. an included config with a larger
    # index in the list will overwrite duplicated GN args for any config with
    # a smaller index.
    for child_config_name in configs:
        graph.add_edge(gn_config_key, _GN_CONFIG.key(child_config_name))

    return gn_config_key

def _config(name = None, args = {}, configs = [], args_file = ""):
    """Define a GN config.

    This method can be used to create commonly used GN config by setting
    the "name" parameter, see //gn_args.star for more examples.
    It can also be used without setting a "name" to specify the gn_args
    property within a builder definition.

    Args:
        name: (string) The name of the GN config.
        args: (dict) GN arg key-value pairs.
        configs: (list) String names of included GN configs.
        args_file: (string) The string path of an imported GN file.

    Returns:
        None if the "name" parameter is set; otherwise, the input for
        the "gn_args" field within a builder definition.
    """
    if name:
        _create_gn_config_node(name, args, configs, args_file)
        return None
    else:
        return struct(
            gn_args = args,
            configs = configs,
            args_file = args_file,
        )

gn_args = struct(
    config = _config,
)

def register_gn_args(builder_group, bucket, builder, gn_args):
    """Register GN args for a builder.

    Internally creates a node of gn_config kind for a builder.
    The builder ID ("bucket/builder") will be used as the name of the node.

    Args:
        builder_group: (string) The builder group name.
        bucket: (string) The bucket name.
        builder: (string) The builder name.
        gn_args: The string name of a GN config, or the return value of
            a gn_args.config method call without setting the "name" parameter.

    Returns:
        A list of generated GN args file paths relative to the per-builder
            output root dir if gn_args is set; None otherwise.
    """
    if gn_args:
        if type(gn_args) == "string":
            gn_args = {"configs": [gn_args]}
        else:
            # If a unnamed config is specified, convert the struct to dict.
            gn_args = {a: getattr(gn_args, a) for a in dir(gn_args)}
        gn_args["builder_group"] = builder_group
        builder_node_key = _create_gn_config_node("{}/{}".format(bucket, builder), **gn_args)
        graph.add_edge(keys.project(), builder_node_key)
        return ["{}/{}/{}".format(bucket, builder, _GN_ARGS_FILE_NAME)]
    else:
        return None

def _generate_gn_args(ctx):
    """Generator callback for generating "gn-args.json" files.

    Args:
        ctx: the context object.
    """
    args_gn_locations = {}
    root_out_dir = per_builder_outputs_config().root_dir
    builder_nodes = graph.children(keys.project(), _GN_CONFIG.kind)
    if builder_nodes:
        resolve_gn_args = _get_gn_args_resolver()
        for node in builder_nodes:
            gn_args_file_path = "{}/{}/{}".format(root_out_dir, node.key.id, _GN_ARGS_FILE_NAME)
            gn_args_dict = resolve_gn_args(node)
            for key in gn_args_dict.keys():
                if not gn_args_dict[key]:
                    gn_args_dict.pop(key)
            ctx.output[gn_args_file_path] = json.indent(json.encode(gn_args_dict), indent = "  ")

            builder_name = node.key.id.rsplit("/", 1)[-1]
            builder_group = node.props.builder_group
            args_gn_locations.setdefault(
                builder_group,
                {},
            )[builder_name] = "{}/{}".format(node.key.id, _GN_ARGS_FILE_NAME)

    locations_file_path = "{}/gn_args_locations.json".format(root_out_dir)
    ctx.output[locations_file_path] = json.indent(
        json.encode(args_gn_locations),
        indent = "  ",
    )

lucicfg.generator(_generate_gn_args)
