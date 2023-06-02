import View from './view'
import Controller from './controller'
import Model from './model'
import Store from './store'
import Template from './template'
import {remove} from './helpers'

export {updateTodo, getTodo, subscribe}

let todo
const subscribers = []

/**
 * Sets up a brand new Todo list.
 *
 * @param {string} name The name of your new to do list.
 */
function Todo(name) {
  this.storage = new Store(name)
  this.model = new Model(this.storage)
  this.template = new Template()
  this.view = new View(this.template)
  this.controller = new Controller(this.model, this.view)
}

function updateTodo() {
  todo = new Todo('todos-vanillajs')
  todo.controller.setView(document.location.hash)
  subscribers.forEach(s => s())
}

function getTodo() {
  return todo
}

function subscribe(cb) {
  subscribers.push(cb)
  return function unsubscribe() {
    remove(subscribers, cb)
  }
}
