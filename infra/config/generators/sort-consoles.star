def _sort_consoles(ctx):
    milo = ctx.output["luci/luci-milo.cfg"]

    # Sort so that the overview consoles appear at the top of the console list
    # The overview consoles specify a title, so checking c.id == c.name will put
    # the overview consoles first
    milo.consoles = sorted(milo.consoles, key = lambda c: (c.id == c.name, c.id))

lucicfg.generator(_sort_consoles)
