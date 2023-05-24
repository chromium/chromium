import 'todomvc-app-css/index.css'
import './app.css'

import {$on} from './helpers'
import {updateTodo} from './todo'

export function onLoad() { // eslint-disable-line import/prefer-default-export
  updateTodo()
}
