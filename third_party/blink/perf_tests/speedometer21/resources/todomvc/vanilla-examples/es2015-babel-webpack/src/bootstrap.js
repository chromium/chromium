/* eslint no-console:0 */
import {onLoad} from './app'
import {$on} from './helpers'

// this is only relevant when using `hot` mode with webpack
// special thanks to Eric Clemmons: https://github.com/ericclemmons/webpack-hot-server-example
const reloading = document.readyState === 'complete'
if (module.hot) {
  module.hot.accept(function(err) {
  })
  if (reloading) {
    onLoad()
  } else {
    bootstrap()
  }
} else {
  bootstrap()
}

function bootstrap() {
  $on(window, 'load', onLoad)
  $on(window, 'hashchange', onLoad)
}
