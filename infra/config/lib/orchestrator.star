# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining orchestrators and compilators."""

load("@stdlib//internal/graph.star", "graph")
load("./html.star", "builder_url")
load("./nodes.star", "nodes")

# The generator in builder_config.star will set the
# chromium_tests_builder_config property on the orchestrator, so load it first
# so that when properties get copied to the compilator the
# chromium_tests_builder_config property gets included
load("./builder_config.star", _ = "builder_config")  # @unused

# infra/infra git revision to use for the compilator_watcher luciexe sub_build
# Used by chromium orchestrators
_COMPILATOR_WATCHER_GIT_REVISION = "27c191f304c8d7329a393d8a69020fc14032c3c3"

# Nodes for the definition of an orchestrator builder
_ORCHESTRATOR = nodes.create_bucket_scoped_node_type("orchestrator")

# Nodes for the definition of a compilator builder
_COMPILATOR = nodes.create_node_type_with_builder_ref("compilator")

# We want to be able to set up experimental orchestrator builders that mirror an
# orchestrator without having to set up a separate pool of compilators, so this
# provides a means of doing so that can't is unlikely to be done inadvertently.
# Experimental orchestrators must have the same properties as the
# non-experimental orchestrator associated with the compilator.
#
# The keys are the bucket-qualified name of a compilator (e.g.
# "try/linux-rel-compilator"), with the values being a container of
# bucket-qualified names of the experimental orchestrators that can use the
# compilator.
_EXPERIMENTAL_ORCHESTRATOR_NAMES_BY_COMPILATOR_NAME = {
}

SOURCELESS_BUILDER_CACHE_NAME = "unused_builder_cache"

def register_orchestrator(bucket, name, builder_group, compilator):
    key = _ORCHESTRATOR.add(bucket, name, props = {
        "bucket": bucket,
        "name": name,
        "builder_group": builder_group,
    })

    _COMPILATOR.add_ref(key, compilator)

def register_compilator(bucket, name):
    _COMPILATOR.add(bucket, name, props = {
        "bucket": bucket,
        "name": name,
    })

def _builder_name(node):
    return "{}/{}".format(node.props.bucket, node.props.name)

def _update_description(builder, additional_description):
    description = builder.description_html
    if description:
        description += "<br/>"
    description += additional_description
    builder.description_html = description

def _get_orchestrator(bucket_name, builder):
    """Get orchestrator details for a buildbucket Builder message.

    Returns:
      If the builder is a orchestrator, a struct with the attributes:
        * name: The bucket-qualified name of the builder (e.g. "try/linux-rel").
        * builder: The buildbucket Builder message.
        * compilator_name: The bucket-qualified name of the associated
          compilator (e.g. "try/linux-rel-compilator").
        * builder_group: The group of the orchestrator.
        * bucket: The name of the LUCI bucket of the orchestrator.
        * simple_name: The non-bucket-qualified name of the orchestrator (e.g.
          "linux-rel").
      Otherwise, None.
    """
    node = _ORCHESTRATOR.get(bucket_name, builder.name)
    if not node:
        return None

    compilator_ref_nodes = graph.children(node.key, _COMPILATOR.ref_kind)

    # This would represent an error in the register code
    if len(compilator_ref_nodes) != 1:
        fail("internal error: orchestrator node {} should have exactly one compilator ref child, got {}"
            .format(_builder_name(node), [n.key.id for n in compilator_ref_nodes]))

    compilator_node = _COMPILATOR.follow_ref(compilator_ref_nodes[0], node)
    return struct(
        name = _builder_name(node),
        builder = builder,
        compilator_name = _builder_name(compilator_node),
        builder_group = node.props.builder_group,
        bucket = bucket_name,
        simple_name = builder.name,
    )

def _get_compilator(bucket_name, builder):
    """Get compilator details for a buildbucket Builder message.

    Returns:
      If the builder is a compilator, a struct with the attributes:
        * name: The bucket-qualified name of the builder (e.g.
          "try/linux-rel-compilator").
        * builder: The buildbucket Builder message.
        * bucket: The name of the LUCI bucket of the compilator.
        * simple_name: The non-bucket-qualified name of the orchestrator (e.g.
          "linux-rel-compilator").
      Otherwise, None.

    Fails:
      If the number of orchestrators referring to the compilator is not 1.
    """
    node = _COMPILATOR.get(bucket_name, builder.name)
    if not node:
        return None

    compilator_name = _builder_name(node)

    orchestrator_nodes = []
    for r in graph.parents(node.key, _COMPILATOR.ref_kind):
        orchestrator_nodes.extend(graph.parents(r.key, _ORCHESTRATOR.kind))

    experimental_orchestrator_names = _EXPERIMENTAL_ORCHESTRATOR_NAMES_BY_COMPILATOR_NAME.get(compilator_name, ())
    orchestrator_nodes = [
        n
        for n in orchestrator_nodes
        if _builder_name(n) not in experimental_orchestrator_names
    ]

    if len(orchestrator_nodes) != 1:
        fail("compilator should have exactly 1 referring orchestrator, got: {}, {}".format(
            _builder_name(node),
            [_builder_name(n) for n in orchestrator_nodes],
        ))

    return struct(
        name = compilator_name,
        builder = builder,
        bucket = bucket_name,
        simple_name = builder.name,
    )

def _builder_link(builder_details):
    return "<a href=\"{}\">{}</a>".format(
        builder_url(builder_details.bucket, builder_details.simple_name),
        builder_details.simple_name,
    )

def _get_orchestrators_and_compilators(ctx):
    """Get the details of the orchestrator and compilator builders.

    The validity of the orchestrator and compilator nodes will be checked and
    fail will be called if they don't satisfy the necessary constraints.

    Returns:
      A list where each element is a struct for a compilator with the
      attributes:
        * compilator: The details for the compilator. See the return
          value of _get_compilator for more information.
        * orchestrator: The details for the non-experimental orchestrator
          associated with the compilator. See the return value of
          _get_orchestrator for more information.
        * experimental_orchestrators: A list of details for any experimental
          orchestrators associated with the compilator. See the return value of
          _get_orchestrator for more information.
    """
    cfg = None
    for f in ctx.output:
        if f.startswith("luci/cr-buildbucket"):
            cfg = ctx.output[f]
            break
    if cfg == None:
        fail("There is no buildbucket configuration file to update properties")

    compilators = []
    orchestrators_by_compilator_name = {}
    experimental_orchestrators_by_compilator_name = {}

    for bucket in cfg.buckets:
        if not proto.has(bucket, "swarming"):
            continue
        bucket_name = bucket.name
        for builder in bucket.swarming.builders:
            compilator = _get_compilator(bucket_name, builder)
            if compilator:
                compilators.append(compilator)
                continue

            orchestrator = _get_orchestrator(bucket_name, builder)
            if orchestrator:
                compilator_name = orchestrator.compilator_name
                if orchestrator.name in _EXPERIMENTAL_ORCHESTRATOR_NAMES_BY_COMPILATOR_NAME.get(compilator_name, ()):
                    experimental_orchestrators_by_compilator_name.setdefault(compilator_name, []).append(orchestrator)
                else:
                    orchestrators_by_compilator_name[compilator_name] = orchestrator

    return [struct(
        compilator = c,
        orchestrator = orchestrators_by_compilator_name[c.name],
        experimental_orchestrators = experimental_orchestrators_by_compilator_name.get(c.name, []),
    ) for c in compilators]

_ALLOWED_COMPILATOR_PROPERTIES = set([
    "builder_group",
    "recipe",
])

def _set_orchestrator_properties(ctx):
    details = _get_orchestrators_and_compilators(ctx)
    for d in details:
        orchestrator = d.orchestrator
        orchestrator_properties = json.decode(orchestrator.builder.properties)

        for o in d.experimental_orchestrators:
            o_properties = json.decode(o.builder.properties)
            if o_properties != orchestrator_properties:
                message = ["experimental orchestrator {!r} must have properties equal to those of {!r}".format(o.name, orchestrator.name)]
                message.append("properties of {!r}: {}".format(o.name, json.indent(json.encode(o_properties), indent = "  ")))
                message.append("properties of {!r}: {}".format(orchestrator.name, json.indent(json.encode(orchestrator_properties), indent = "  ")))
                fail("\n".join(message))

        compilator = d.compilator
        orchestrator_caches = []
        for cache in orchestrator.builder.caches:
            # The sourceless builder cache is an optimization to speed up the
            # process of the orchestrator getting assigned a bot since it won't
            # be storing anything in the builder cache to preserve between runs,
            # so it shouldn't be moved to the compilator
            if cache.name == SOURCELESS_BUILDER_CACHE_NAME:
                orchestrator_caches.append(cache)

            else:
                # Other caches should (probably?) be moved to the compilator. At
                # the time of writing, the only caches that would be transferred
                # are the xcode caches, which are added by a parameter that also
                # sets the xcode_build_version property.
                compilator.builder.caches.append(cache)
        orchestrator.builder.caches = orchestrator_caches
        _update_description(
            compilator.builder,
            ("This is the compilator half of an orchestrator + compilator pair of builders." +
             " The orchestrator is <a href=\"{}\">{}</a>.".format(
                 builder_url(orchestrator.bucket, orchestrator.simple_name),
                 orchestrator.simple_name,
             )),
        )

        compilator_properties = json.decode(compilator.builder.properties)
        invalid_compilator_properties = [p for p in compilator_properties if p not in _ALLOWED_COMPILATOR_PROPERTIES]
        if invalid_compilator_properties:
            fail("compilator {!r} has forbidden properties {},".format(compilator.builder.name, invalid_compilator_properties) +
                 " set the corresponding attributes on the orchestrator instead" +
                 " (if the property was generated by default, modify compilator_builder to set corresponding attributes to a value that doesn't generate the properties)")

        orchestrator_properties["$build/chromium_orchestrator"] = {
            "compilator": compilator.simple_name,
            "compilator_watcher_git_revision": _COMPILATOR_WATCHER_GIT_REVISION,
        }
        encoded_orchestrator_properties = json.encode(orchestrator_properties)
        orchestrator.builder.properties = encoded_orchestrator_properties
        _update_description(
            orchestrator.builder,
            ("This is the orchestrator half of an orchestrator + compilator pair of builders." +
             " The compilator is {}.".format(_builder_link(compilator))),
        )

        for o in d.experimental_orchestrators:
            o.builder.properties = encoded_orchestrator_properties
            _update_description(
                o.builder,
                "This is an experimental orchestrator making use of compilator {}.".format(_builder_link(compilator)),
            )
            _update_description(
                compilator.builder,
                "It is also the compilator for experimental orchestrator {}.".format(_builder_link(o)),
            )

lucicfg.generator(_set_orchestrator_properties)
