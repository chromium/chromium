# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining orchestrators and compilators."""

load("@stdlib//internal/graph.star", "graph")
load("./builder_url.star", "builder_url")
load("./nodes.star", "nodes")

# The generator in builder_config.star will set the
# chromium_tests_builder_config property on the orchestrator, so load it first
# so that when properties get copied to the compilator the
# chromium_tests_builder_config property gets included
load("./builder_config.star", _ = "builder_config")  # @unused

# infra/infra git revision to use for the compilator_watcher luciexe sub_build
# Used by chromium orchestrators
_COMPILATOR_WATCHER_GIT_REVISION = "7809a690bbd935bcb3b4d922e24cabe168aaabc8"

# Nodes for the definition of an orchestrator builder
_ORCHESTRATOR = nodes.create_bucket_scoped_node_type("orchestrator")

# Nodes for the definition of a compilator builder
_COMPILATOR = nodes.create_node_type_with_builder_ref("compilator")

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

    orchestrator_nodes = []
    for r in graph.parents(node.key, _COMPILATOR.ref_kind):
        orchestrator_nodes.extend(graph.parents(r.key, _ORCHESTRATOR.kind))

    if len(orchestrator_nodes) != 1:
        fail("compilator should have exactly 1 referring orchestrator, got: {}".format(
            _builder_name(node),
            [_builder_name(n) for n in orchestrator_nodes],
        ))

    return struct(
        name = _builder_name(node),
        builder = builder,
        bucket = bucket_name,
        simple_name = builder.name,
    )

def _get_orchestrators_and_compilators(ctx):
    """Get the details of the orchestrator and compilator builders.

    The validity of the orchestrator and compilator nodes will be checked and
    fail will be called if they don't satisfy the necessary constraints.

    Returns:
      A 2-tuple:
        * A list of details for orchestrator builders. See the return value of
          _get_orchestrator for more information.
        * A dict mapping bucket-qualified name (e.g. "try/linux-rel-compilator")
          of compilator builders to the details for that builder. See the return
          value of _get_compilator for more information.
    """
    cfg = None
    for f in ctx.output:
        if f.startswith("luci/cr-buildbucket"):
            cfg = ctx.output[f]
            break
    if cfg == None:
        fail("There is no buildbucket configuration file to update properties")

    orchestrators = []
    compilator_by_name = {}

    for bucket in cfg.buckets:
        bucket_name = bucket.name
        for builder in bucket.swarming.builders:
            compilator = _get_compilator(bucket_name, builder)
            if compilator:
                compilator_by_name[compilator.name] = compilator
                continue

            orchestrator = _get_orchestrator(bucket_name, builder)
            if orchestrator:
                orchestrators.append(orchestrator)

    return orchestrators, compilator_by_name

def _set_orchestrator_properties(ctx):
    orchestrators, compilators_by_name = _get_orchestrators_and_compilators(ctx)
    for orchestrator in orchestrators:
        orchestrator_properties = json.decode(orchestrator.builder.properties)

        compilator = compilators_by_name[orchestrator.compilator_name]
        compilator_properties = dict(orchestrator_properties)
        compilator_properties.update(json.decode(compilator.builder.properties))
        compilator_properties["orchestrator"] = {
            "builder_group": orchestrator.builder_group,
            "builder_name": orchestrator.simple_name,
        }
        compilator.builder.properties = json.encode(compilator_properties)
        _update_description(
            compilator.builder,
            ("This is the compilator half of an orchestrator + compilator pair of builders." +
             " The orchestrator is <a href=\"{}\">{}</a>.".format(
                 builder_url(orchestrator.bucket, orchestrator.simple_name),
                 orchestrator.simple_name,
             )),
        )

        orchestrator_properties["$build/chromium_orchestrator"] = {
            "compilator": compilator.simple_name,
            "compilator_watcher_git_revision": _COMPILATOR_WATCHER_GIT_REVISION,
        }
        orchestrator.builder.properties = json.encode(orchestrator_properties)
        _update_description(
            orchestrator.builder,
            ("This is the orchestrator half of an orchestrator + compilator pair of builders." +
             " The compilator is <a href=\"{}\">{}</a>.".format(
                 builder_url(compilator.bucket, compilator.simple_name),
                 compilator.simple_name,
             )),
        )

lucicfg.generator(_set_orchestrator_properties)
