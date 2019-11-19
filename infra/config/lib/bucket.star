def var(*, default):
  var = lucicfg.var(default = default)
  def builder(builder):
    return '{}/{}'.format(var.get(), builder)
  return struct(
      builder = builder,
      get = var.get,
      set = var.set,
  )
