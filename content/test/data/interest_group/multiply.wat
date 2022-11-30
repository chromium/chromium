(module
  (func (export "compute") (param f64) (result f64)
    local.get 0
    f64.const 4
    f64.mul
  )
)
